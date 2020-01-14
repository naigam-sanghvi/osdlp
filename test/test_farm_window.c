/*
 *  Open Space Data Link Protocol
 *
 *  Copyright (C) 2020 Libre Space Foundation (https://libre.space)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "test.h"

void
test_vr(void **state)
{
	uint16_t      up_chann_item_size = TC_MAX_FRAME_LEN;
	uint16_t      up_chann_capacity = 10;
	uint16_t      down_chann_item_size = sizeof(struct clcw_frame);
	uint16_t      down_chann_capacity = 10;
	uint16_t      sent_item_size = sizeof(struct local_queue_item);
	uint16_t      sent_capacity = 10;
	uint16_t      wait_item_size = sizeof(struct tc_transfer_frame);
	uint16_t      rx_item_size = MAX_SDU_SIZE;
	uint16_t      rx_capacity = 10;

	uint16_t      scid = 101;
	uint16_t      max_frame_size = TC_MAX_FRAME_LEN;
	uint8_t       vcid = 1;
	uint8_t       mapid = 1;
	tc_crc_flag_t crc = TC_CRC_PRESENT;
	tc_seg_hdr_t  seg_hdr = TC_SEG_HDR_PRESENT;
	tc_bypass_t   bypass = TYPE_B;
	tc_ctrl_t     ctrl = TC_DATA;
	uint16_t      fop_slide_wnd = 5;
	fop_state_t   fop_init_st = FOP_STATE_INIT;
	uint16_t      fop_t1_init = 1;
	uint16_t      fop_timeout_type = 0;
	uint8_t       fop_tx_limit = 10;
	farm_state_t  farm_init_st = FARM_STATE_OPEN;
	uint8_t       farm_wnd_width = 10;
	notification_t notif;
	farm_result_t  farm_ret;

	setup_queues(up_chann_item_size,
	             up_chann_capacity,
	             down_chann_item_size,
	             down_chann_capacity,
	             sent_item_size,
	             sent_capacity,
	             wait_item_size,
	             rx_item_size,
	             rx_capacity);                           /*Prepare queues*/

	struct clcw_frame clcw;
	int ret;
	setup_configs(&tc_tx, &tc_rx,
	              &cop_tx, &cop_rx,
	              &fop, &farm,
	              scid, max_frame_size,
	              vcid, mapid, crc,
	              seg_hdr, bypass,
	              ctrl, fop_slide_wnd,
	              fop_init_st, fop_t1_init,
	              fop_timeout_type, fop_tx_limit,
	              farm_init_st, farm_wnd_width);         /*Prepare config structs*/
	uint8_t buf[MAX_SDU_SIZE];
	int size = 150;
	for (int i = 0; i < size; i++) {
		buf[i] = rand() % 256;
	}

	/* Initiate with clcw */
	notif = initiate_with_clcw(&tc_tx);
	assert_int_equal(notif, ACCEPT_TX);


	/* Prepare clcw */
	ret = prepare_clcw(&tc_rx_unseg, &clcw);

	/* Handle response */
	notif = handle_clcw(&tc_tx, &clcw);
	assert_int_equal(notif, POSITIVE_DIR);

	ret = prepare_typea_data_frame(&tc_tx, buf, size);
	assert_int_equal(0, ret);

	/* Transmit two frames */
	notif = tc_transmit(&tc_tx, buf, size);
	assert_int_equal(notif, ACCEPT_TX);

	notif = tc_transmit(&tc_tx, buf, size);
	assert_int_equal(notif, ACCEPT_TX);
	assert_int_equal(tc_tx.cop_cfg.fop.vs, 2);

	/* Receive two frames */

	ret = dequeue(&uplink_channel, test_util);
	assert_int_equal(ret, 0);

	farm_ret = tc_receive(test_util, MAX_FRAME_LEN);
	assert_int_equal(farm_ret, COP_ENQ);

	ret = prepare_clcw(&tc_rx, &clcw);
	assert_int_equal(clcw.report_value, 1);

	notif = handle_clcw(&tc_tx, &clcw);
	assert_int_equal(notif, ACCEPT_TX);

	ret = dequeue(&uplink_channel, test_util);
	assert_int_equal(ret, 0);

	farm_ret = tc_receive(test_util, MAX_FRAME_LEN);
	assert_int_equal(farm_ret, COP_ENQ);

	ret = prepare_clcw(&tc_rx, &clcw);
	assert_int_equal(clcw.report_value, 2);

	notif = handle_clcw(&tc_tx, &clcw);
	assert_int_equal(notif, ACCEPT_TX);

	/* Terminate service */
	notif = terminate_ad(&tc_tx);
	assert_int_equal(notif, POSITIVE_DIR);

	/*Set new high VS*/
	notif = set_new_vs(&tc_tx, 15);
	assert_int_equal(notif, POSITIVE_DIR);

	/*Initiate without clcw*/
	notif = initiate_no_clcw(&tc_tx);
	assert_int_equal(notif, POSITIVE_DIR);

	/* Transmit frame with wrong VS*/
	notif = tc_transmit(&tc_tx, buf, size);
	assert_int_equal(notif, ACCEPT_TX);

	ret = dequeue(&uplink_channel, test_util);
	assert_int_equal(ret, 0);

	farm_ret = tc_receive(test_util, MAX_FRAME_LEN);
	assert_int_equal(farm_ret, COP_DISCARD);

	ret = prepare_clcw(&tc_rx, &clcw);
	assert_int_equal(clcw.lockout, 1);

	notif = terminate_ad(&tc_tx);
	assert_int_equal(notif, POSITIVE_DIR);


	/* Initiate with unlock */
	notif = set_new_vs(&tc_tx, 2);
	assert_int_equal(notif, POSITIVE_DIR);

	notif = initiate_with_unlock(&tc_tx);
	assert_int_equal(notif, ACCEPT_DIR);

	ret = dequeue(&uplink_channel, test_util);
	assert_int_equal(ret, 0);

	farm_ret = tc_receive(test_util, MAX_FRAME_LEN);
	assert_int_equal(farm_ret, COP_OK);

	ret = prepare_clcw(&tc_rx, &clcw);
	assert_int_equal(clcw.lockout, 0);

	notif = handle_clcw(&tc_tx, &clcw);
	assert_int_equal(notif, POSITIVE_DIR);

	ret = prepare_typea_data_frame(&tc_tx, buf, size);
	assert_int_equal(0, ret);

	notif = tc_transmit(&tc_tx, buf, size);
	assert_int_equal(notif, ACCEPT_TX);

	ret = dequeue(&uplink_channel, test_util);
	assert_int_equal(ret, 0);

	farm_ret = tc_receive(test_util, MAX_FRAME_LEN);
	assert_int_equal(farm_ret, COP_ENQ);

	ret = prepare_clcw(&tc_rx, &clcw);
	assert_int_equal(clcw.lockout, 0);



	/* Transmit frames with wrong VS */
	notif = terminate_ad(&tc_tx);
	assert_int_equal(notif, POSITIVE_DIR);

	notif = set_new_vs(&tc_tx, 254);
	assert_int_equal(notif, POSITIVE_DIR);

	notif = initiate_with_setvr(&tc_tx, 254);
	assert_int_equal(notif, ACCEPT_DIR);

	ret = dequeue(&uplink_channel, test_util);
	assert_int_equal(ret, 0);

	farm_ret = tc_receive(test_util, MAX_FRAME_LEN);
	assert_int_equal(farm_ret, COP_OK);

	ret = prepare_clcw(&tc_rx, &clcw);
	assert_int_equal(clcw.lockout, 0);

	notif = handle_clcw(&tc_tx, &clcw);
	assert_int_equal(notif, POSITIVE_DIR);

	/* Transmit a packet */

	/* Set VS to 254 */
	ret = prepare_typea_data_frame(&tc_tx, buf, size);
	assert_int_equal(0, ret);
	tc_tx.cop_cfg.fop.vs = 255;	/*Inside positive window of FARM */
	ret = tc_pack(&tc_tx, test_util, buf, size);
	tx_queue_enqueue(test_util, 1);

	ret = dequeue(&uplink_channel, test_util);
	assert_int_equal(ret, 0);

	farm_ret = tc_receive(test_util, MAX_FRAME_LEN);
	assert_int_equal(farm_ret, COP_DISCARD);

	ret = prepare_clcw(&tc_rx, &clcw);
	assert_int_equal(clcw.rt, 1);



	/* Set VS to 0 */
	ret = prepare_typea_data_frame(&tc_tx, buf, size);
	assert_int_equal(0, ret);
	tc_tx.cop_cfg.fop.vs = 0;	/*Inside positive window of FARM */
	ret = tc_pack(&tc_tx, test_util, buf, size);
	tx_queue_enqueue(test_util, 1);

	ret = dequeue(&uplink_channel, test_util);
	assert_int_equal(ret, 0);

	farm_ret = tc_receive(test_util, MAX_FRAME_LEN);
	assert_int_equal(farm_ret, COP_DISCARD);

	ret = prepare_clcw(&tc_rx, &clcw);
	assert_int_equal(clcw.report_value, 254);


	/* Send two correct packets */
	tc_tx.cop_cfg.fop.vs = 254;
	ret = prepare_typea_data_frame(&tc_tx, buf, size);
	assert_int_equal(0, ret);

	notif = tc_transmit(&tc_tx, buf, size);
	assert_int_equal(notif, ACCEPT_TX);

	ret = dequeue(&uplink_channel, test_util);
	assert_int_equal(ret, 0);

	farm_ret = tc_receive(test_util, MAX_FRAME_LEN);
	assert_int_equal(farm_ret, COP_ENQ);

	ret = prepare_clcw(&tc_rx, &clcw);
	assert_int_equal(clcw.report_value, 255);

	notif = handle_clcw(&tc_tx, &clcw);

	/* Transmit a packet */

	ret = prepare_typea_data_frame(&tc_tx, buf, size);
	assert_int_equal(0, ret);

	notif = tc_transmit(&tc_tx, buf, size);
	assert_int_equal(notif, ACCEPT_TX);

	ret = dequeue(&uplink_channel, test_util);
	assert_int_equal(ret, 0);

	farm_ret = tc_receive(test_util, MAX_FRAME_LEN);
	assert_int_equal(farm_ret, COP_ENQ);

	ret = prepare_clcw(&tc_rx, &clcw);
	assert_int_equal(clcw.report_value, 0);

	notif = handle_clcw(&tc_tx, &clcw);

	/* Transmit a packet */

	ret = prepare_typea_data_frame(&tc_tx, buf, size);
	assert_int_equal(0, ret);

	notif = tc_transmit(&tc_tx, buf, size);
	assert_int_equal(notif, ACCEPT_TX);

	ret = dequeue(&uplink_channel, test_util);
	assert_int_equal(ret, 0);

	farm_ret = tc_receive(test_util, MAX_FRAME_LEN);
	assert_int_equal(farm_ret, COP_ENQ);

	ret = prepare_clcw(&tc_rx, &clcw);
	assert_int_equal(clcw.report_value, 1);

	notif = handle_clcw(&tc_tx, &clcw);

	/* Set VS to 255 */

	tc_tx.cop_cfg.fop.vs = 255;	/*Inside negative window of FARM */
	ret = tc_pack(&tc_tx, test_util, buf, size);
	tx_queue_enqueue(test_util, 1);

	ret = dequeue(&uplink_channel, test_util);
	assert_int_equal(ret, 0);

	farm_ret = tc_receive(test_util, MAX_FRAME_LEN);
	assert_int_equal(farm_ret, COP_DISCARD);

	ret = prepare_clcw(&tc_rx, &clcw);
	assert_int_equal(clcw.report_value, 1);

	/* Set VS to 0 */

	ret = prepare_typea_data_frame(&tc_tx, buf, size);
	assert_int_equal(0, ret);
	tc_tx.cop_cfg.fop.vs = 0;	/*Inside positive window of FARM */
	ret = tc_pack(&tc_tx, test_util, buf, size);
	tx_queue_enqueue(test_util, 1);

	ret = dequeue(&uplink_channel, test_util);
	assert_int_equal(ret, 0);

	farm_ret = tc_receive(test_util, MAX_FRAME_LEN);
	assert_int_equal(farm_ret, COP_DISCARD);

	ret = prepare_clcw(&tc_rx, &clcw);
	assert_int_equal(clcw.report_value, 1);
}

