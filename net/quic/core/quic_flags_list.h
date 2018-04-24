// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate values. The following line silences a
// presubmit warning that would otherwise be triggered by this:
// no-include-guard-because-multiply-included

// This file contains the list of QUIC protocol flags.

// Time period for which a given connection_id should live in the time-wait
// state.
QUIC_FLAG(int64_t, FLAGS_quic_time_wait_list_seconds, 200)

// Currently, this number is quite conservative.  The max QPS limit for an
// individual server silo is currently set to 1000 qps, though the actual max
// that we see in the wild is closer to 450 qps.  Regardless, this means that
// the longest time-wait list we should see is 200 seconds * 1000 qps, 200000.
// Of course, there are usually many queries per QUIC connection, so we allow a
// factor of 3 leeway.
//
// Maximum number of connections on the time-wait list. A negative value implies
// no configured limit.
QUIC_FLAG(int64_t, FLAGS_quic_time_wait_list_max_connections, 600000)

// Enables server-side support for QUIC stateless rejects.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_enable_quic_stateless_reject_support,
          true)

// If true, require handshake confirmation for QUIC connections, functionally
// disabling 0-rtt handshakes.
// TODO(rtenneti): Enable this flag after CryptoServerTest's are fixed.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_require_handshake_confirmation,
          false)

// If true, disable pacing in QUIC.
QUIC_FLAG(bool, FLAGS_quic_disable_pacing_for_perf_tests, false)

// If true, QUIC will use cheap stateless rejects without creating a full
// connection.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_use_cheap_stateless_rejects,
          true)

// If true, QUIC respect HTTP2 SETTINGS frame rather than always close the
// connection.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_respect_http2_settings_frame,
          true)

// If true, allows packets to be buffered in anticipation of a future CHLO, and
// allow CHLO packets to be buffered until next iteration of the event loop.
QUIC_FLAG(bool, FLAGS_quic_allow_chlo_buffering, true)

// If true, GFE sends SETTINGS_MAX_HEADER_LIST_SIZE to the client at the
// beginning of a connection.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_send_max_header_list_size, true)

// If greater than zero, mean RTT variation is multiplied by the specified
// factor and added to the congestion window limit.
QUIC_FLAG(double, FLAGS_quic_bbr_rtt_variation_weight, 0.0f)

// Congestion window gain for QUIC BBR during PROBE_BW phase.
QUIC_FLAG(double, FLAGS_quic_bbr_cwnd_gain, 2.0f)

// Add the equivalent number of bytes as 3 TCP TSO segments to QUIC's BBR CWND.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr_add_tso_cwnd, false)

// Simplify QUIC\'s adaptive time loss detection to measure the necessary
// reordering window for every spurious retransmit.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_fix_adaptive_time_loss, false)

// Allows the 3RTO QUIC connection option to close a QUIC connection after
// 3RTOs if there are no open streams.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_3rtos, false)

// If true, enable experiment for testing PCC congestion-control.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_pcc, false)

// When true, defaults to BBR congestion control instead of Cubic.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_default_to_bbr, false)

// Allow a new rate based recovery in QUIC BBR to be enabled via connection
// option.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr_rate_recovery, false)

// If buffered data in QUIC stream is less than this threshold, buffers all
// provided data or asks upper layer for more data.
QUIC_FLAG(uint32_t, FLAGS_quic_buffered_data_threshold, 8192u)

// Max size of data slice in bytes for QUIC stream send buffer.
QUIC_FLAG(uint32_t, FLAGS_quic_send_buffer_max_data_slice_size, 4096u)

// If true, QUIC supports both QUIC Crypto and TLS 1.3 for the handshake
// protocol.
QUIC_FLAG(bool, FLAGS_quic_supports_tls_handshake, false)

// Allow QUIC to accept initial packet numbers that are random, not 1.
QUIC_FLAG(bool, FLAGS_quic_restart_flag_quic_enable_accept_random_ipn, false)

// If true, enable QUIC v43.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_version_43, false)

// Enables 3 new connection options to make PROBE_RTT more aggressive
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr_less_probe_rtt, false)

// If true, limit quic stream length to be below 2^62.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_stream_too_long, false)

// When true, enables the 1TLP connection option to configure QUIC to send one
// TLP instead of 2.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_one_tlp, false)

// When true, allows two connection options to run experiments with using max
// ack delay as described in QUIC IETF.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_max_ack_delay, false)

// If true, enable QUIC v99.
QUIC_FLAG(bool, FLAGS_quic_enable_version_99, false)

// If true, enable QUIC version 42.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_version_42_2, true)

QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_disable_version_37, true)
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_disable_version_38, true)
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_disable_version_41, false)

// Delays construction of QuicCryptoServerStream::HandshakerDelegate
// until QuicCryptoServerStream::OnSuccessfulVersionNegotiation is called
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_delay_quic_server_handshaker_construction,
          true)
// Controls whether QuicConnection::OnProtocolVersionMismatch calls
// QuicFramer::set_version before or after calling
// OnSuccessfulVersionNegotiation.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_store_version_before_signalling,
          true)

// When true, enable connection options to have no min TLP and RTO,
// and also allow IETF style TLP.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_max_ack_delay2, false)

// If true, MemSlices in the send buffer is freed out of order.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_free_mem_slice_out_of_order,
          false)

// If true, framer will process and report ack frame incrementally.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_use_incremental_ack_processing3,
          false)

// If true, Http2FrameDecoderAdapter will pass decoded HTTP/2 SETTINGS through
// the SpdyFramerVisitorInterface callback OnSetting(), which will also accept
// unknown SETTINGS IDs.
QUIC_FLAG(bool, FLAGS_quic_restart_flag_http2_propagate_unknown_settings, true)

// If true, enable fast path in QuicStream::OnStreamDataAcked.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_fast_path_on_stream_data_acked,
          false)

// If true, fix a use-after-free bug caused by writing an out-of-order queued
// packet.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_fix_write_out_of_order_queued_packet_crash,
    true)

// If true, QUIC streams are registered in the QuicStream constructor instead
// of in the QuicSpdyStream constructor.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_register_streams_early2, true)

// If this flag and
// FLAGS_quic_reloadable_flag_quic_fix_write_out_of_order_queued_packet_crash
// are both ture, QUIC will clear queued packets before sending connectivity
// probing packets.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_clear_queued_packets_before_sending_connectivity_probing,
    false)

// When true, this flag has QuicConnection call
// QuicConnectionVisitorInterface::OnSuccessfulVersionNegotiation earlier when
// processing the packet header.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_server_early_version_negotiation,
          true)

// If true, QUIC will always discard outgoing packets after connection close.
// Currently out-of-order outgoing packets are not discarded
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_always_discard_packets_after_close,
          true)

// If true, stop sending a redundant PING every 20 acks.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_remove_redundant_ping, true)

// If true, when a stream is reset by peer with error, it should not be added to
// zombie streams.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_reset_stream_is_not_zombie,
          true)

// If true, when a packet write for connectivity probe does not complete
// successfully synchronously, connection will not be affected, i.e., blocked or
// closed, if the probing packet writer is not the default writer.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_handle_write_results_for_connectivity_probe,
    true)

// If true, a separate QuicAlarm in QuicConnection is used to trigger
// OnPathDegrading() instead of using retransmission_alarm_.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_path_degrading_alarm2, true)

// Remove special logic for headers stream from QuicWriteBlockedList and
// replace it with a static streams map.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_register_static_streams, false)

// Base the QUIC crypto retransmission timer on the last sent crypto packet.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_better_crypto_retransmission,
          true)

// If true, enable server proxy support in QUIC.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_server_proxy, false)

// If true, compare offset with last byte acked to determine whether it is
// disjoint before calling IntervalSet::IsDisjoint.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_fast_is_disjoint, false)

// If true, enable fast path in QuicStreamSequencerBuffer::OnStreamData.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_fast_path_on_stream_data, false)

// When true, set the initial congestion control window from connection options
// in QuicSentPacketManager rather than TcpCubicSenderBytes.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_unified_iw_options, false)

// If true, check again that the writer isn\'t blocked before calling
// QuicConnection::OnCanWrite from WriteAndBundleAcksIfNotBlocked
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_is_write_blocked, true)

// If true, use the send alarm instead of the resume_writes alarm in
// QuicConnection.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_unified_send_alarm, false)

// If true, do not use QUIC's MSPC handshake tag and instead require MIDS.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_no_mspc, false)

// If true, simplify pacing sender logic.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_simplify_pacing_sender, false)
