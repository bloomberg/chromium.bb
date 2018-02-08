// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate values.

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

// If true, enable QUIC v42.
QUIC_FLAG(bool, FLAGS_quic_enable_version_42, false)

// If buffered data in QUIC stream is less than this threshold, buffers all
// provided data or asks upper layer for more data.
QUIC_FLAG(uint32_t, FLAGS_quic_buffered_data_threshold, 8192u)

// Max size of data slice in bytes for QUIC stream send buffer.
QUIC_FLAG(uint32_t, FLAGS_quic_send_buffer_max_data_slice_size, 4096u)

// If true, QUIC supports both QUIC Crypto and TLS 1.3 for the handshake
// protocol.
QUIC_FLAG(bool, FLAGS_quic_supports_tls_handshake, false)

// If true, QUIC can take ownership of data provided in a reference counted
// memory to avoid data copy.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_use_mem_slices, false)

// Allow QUIC to accept initial packet numbers that are random, not 1.
QUIC_FLAG(bool, FLAGS_quic_restart_flag_quic_enable_accept_random_ipn, false)

// If true, enable QUIC v43.
QUIC_FLAG(bool, FLAGS_quic_enable_version_43, false)

// Enables 3 new connection options to make PROBE_RTT more aggressive
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr_less_probe_rtt, false)

// If true, server will send a connectivity probing to the source address of the
// received connectivity probing. Otherwise, server will treat connectivity
// probing packet as normal packet
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_server_reply_to_connectivity_probing,
          true)

// If true, calculate stream sequencer buffer block count in a way that
// guaranteed to be 2048.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_fix_sequencer_buffer_block_count2,
          true)

// If true, fixes for the two bugs described in crbug.com/723604 will be
// enabled.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_2rtt_drop_client_cached_certs,
          true)

// If true, limit quic stream length to be below 2^62.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_stream_too_long, false)

// When true, enables the 1TLP connection option to configure QUIC to send one
// TLP instead of 2.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_one_tlp, false)

// If true, stream sequencer buffer allows receiving overlapping stream data.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_allow_receiving_overlapping_data,
          true)

// If true, QuicStreamSendBuffer keeps track of the slice which next write
// should get data from if writing new data.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_use_write_index, false)

// If true, when WINDOW_UPDATE is received, add stream to session's write
// blocked list and let session unblock it later.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_streams_unblocked_by_session,
          false)

// If true, inspects CHLO packets for indicator tags to allow early session
// creation.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_inspect_chlo_tags, false)

// When true, ignore the specified ack delay if it causes the RTT sample to be
// less than min_rtt.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_min_rtt_ack_delay, false)

// If true, plugin control frame manager to QuicSession, and let it manage sent
// control frames.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_use_control_frame_manager,
          false)
// When true, allows two connection options to run experiments with using max
// ack delay as described in QUIC IETF.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_max_ack_delay, false)

// If ture, sender will close connection when there are too many outstanding
// sent packets
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_close_session_on_too_many_outstanding_sent_packets,
    false)

// If true, enable QUIC v99.
QUIC_FLAG(bool, FLAGS_quic_enable_version_99, false)

// If both this flag and
// gfe2_reloadable_flag_quic_allow_receiving_overlapping_data are
// true, enable QUIC version 42.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_version_42, false)
