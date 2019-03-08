// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate values. The following line silences a
// presubmit warning that would otherwise be triggered by this:
// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

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

// If true, allows packets to be buffered in anticipation of a future CHLO, and
// allow CHLO packets to be buffered until next iteration of the event loop.
QUIC_FLAG(bool, FLAGS_quic_allow_chlo_buffering, true)

// If greater than zero, mean RTT variation is multiplied by the specified
// factor and added to the congestion window limit.
QUIC_FLAG(double, FLAGS_quic_bbr_rtt_variation_weight, 0.0f)

// Congestion window gain for QUIC BBR during PROBE_BW phase.
QUIC_FLAG(double, FLAGS_quic_bbr_cwnd_gain, 2.0f)

// Simplify QUIC\'s adaptive time loss detection to measure the necessary
// reordering window for every spurious retransmit.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_fix_adaptive_time_loss, false)

// When true, defaults to BBR congestion control instead of Cubic.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_default_to_bbr, false)

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
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_version_43, true)

// Enables 3 new connection options to make PROBE_RTT more aggressive
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr_less_probe_rtt, false)

// If true, enable QUIC v99.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_version_99, false)

// When true, set the initial congestion control window from connection options
// in QuicSentPacketManager rather than TcpCubicSenderBytes.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_unified_iw_options, false)

// Number of packets that the pacing sender allows in bursts during pacing.
QUIC_FLAG(int32_t, FLAGS_quic_lumpy_pacing_size, 1)

// Congestion window fraction that the pacing sender allows in bursts during
// pacing.
QUIC_FLAG(double, FLAGS_quic_lumpy_pacing_cwnd_fraction, 0.25f)

// Default enables QUIC ack decimation and adds a connection option to disable
// it.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_ack_decimation, false)

// If true, QUIC offload pacing when using USPS as egress method.
QUIC_FLAG(bool, FLAGS_quic_restart_flag_quic_offload_pacing_to_usps2, false)

// Max time that QUIC can pace packets into the future in ms.
QUIC_FLAG(int32_t, FLAGS_quic_max_pace_time_into_future_ms, 10)

// Smoothed RTT fraction that a connection can pace packets into the future.
QUIC_FLAG(double, FLAGS_quic_pace_time_into_future_srtt_fraction, 0.125f)

// If true, enable QUIC v44.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_version_44, true)

// Stop checking QuicUnackedPacketMap::HasUnackedRetransmittableFrames and
// instead rely on the existing check that bytes_in_flight > 0
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_optimize_inflight_check, false)

// When you\'re app-limited entering recovery, stay app-limited until you exit
// recovery in QUIC BBR.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr_app_limited_recovery, false)

// If true, stop resetting ideal_next_packet_send_time_ in pacing sender.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_donot_reset_ideal_next_packet_send_time,
    false)

// If true, enable experiment for testing PCC congestion-control.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_pcc3, false)

// When true, ensure BBR allows at least one MSS to be sent in response to an
// ACK in packet conservation.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr_one_mss_conservation, false)

// Add 3 connection options to decrease the pacing and CWND gain in QUIC BBR
// STARTUP.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr_slower_startup3, false)

// When true, the LOSS connection option allows for 1/8 RTT of reording instead
// of the current 1/8th threshold which has been found to be too large for fast
// loss recovery.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_eighth_rtt_loss_detection,
          false)

// Enables the BBQ5 connection option, which forces saved aggregation values to
// expire when the bandwidth increases more than 25% in QUIC BBR STARTUP.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr_slower_startup4, false)

// When true and the BBR9 connection option is present, BBR only considers
// bandwidth samples app-limited if they're not filling the pipe.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr_flexible_app_limited, false)

// If true, calling StopReading() on a level-triggered QUIC stream sequencer
// will cause the sequencer to discard future data.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_stop_reading_when_level_triggered,
          false)

// If true, QuicSession::HasPendingCryptoData checks whether the crypto stream's
// send buffer is empty. This flag fixes a bug where the retransmission alarm
// mode is wrong for the first CHLO packet.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_fix_has_pending_crypto_data,
          true)

// When true, fix initialization and updating of
// |time_of_first_packet_sent_after_receiving_| in QuicConnection.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_fix_time_of_first_packet_sent_after_receiving,
    true)

// When the STMP connection option is sent by the client, timestamps in the QUIC
// ACK frame are sent and processed.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_send_timestamps, false)

// If true, dispatcher passes in a single version when creating a server
// connection, such that version negotiation is not supported in connection.
QUIC_FLAG(bool,
          FLAGS_quic_restart_flag_quic_no_server_conn_ver_negotiation2,
          false)

// If true, enable QUIC version 46.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_version_46, true)

// When in STARTUP and recovery, do not add bytes_acked to QUIC BBR's CWND in
// CalculateCongestionWindow()
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_bbr_no_bytes_acked_in_startup_recovery,
    false)

// If true, use common code for checking whether a new stream ID may be
// allocated.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_use_common_stream_check, false)

// If true, QuicEpollClock::Now() will monotonically increase.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_monotonic_epoll_clock, false)

// If true, a client connection would be closed when a version negotiation
// packet is received. It would be the higher layer's responsibility to do the
// reconnection.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_no_client_conn_ver_negotiation,
          false)

// If true, public reset packets sent from GFE will include a kEPID tag.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_fix_spurious_ack_alarm, false)

// If true, QuicSpdyStream::WritevBody() will convert iovs into QuicMemSliceSpan
// and call WriteMemSlices instead.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_call_write_mem_slices, true)

// If true, enables the BBS4 and BBS5 connection options, which reduce BBR's
// pacing rate in STARTUP as more losses occur as a fraction of CWND.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_bbr_startup_rate_reduction,
          false)

// If true, disconnected quic connection will not be added to dispatcher's write
// blocked list.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_connection_do_not_add_to_write_blocked_list_if_disconnected,
    true)

// If true, QuicPacketCreator::SetTransmissionType will set the transmission
// type of the next successfully added frame.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_set_transmission_type_for_next_frame,
          true)

// If true, log leaf cert subject name into warning log.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_log_cert_name_for_empty_sct,
          true)

// If true, QuicCryptoServerConfig will correctly rotate configs based on
// primary time.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_fix_config_rotation, true)

// If true, use numeric_limits<uint64_t>::max() to represent uninitialized
// packet number.
QUIC_FLAG(bool, FLAGS_quic_restart_flag_quic_uint64max_uninitialized_pn, true)

// If true, enable QUIC version 47 which adds CRYPTO frames.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_version_47, false)

// If true, disable QUIC version 39.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_disable_version_39, false)

// If true, simplifies the implementation of
// QuicFramer::BuildConnectivityProbingPacket().
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_simplify_build_connectivity_probing_packet,
    true)

// If true, use one loss algorithm per encryption level.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_use_uber_loss_algorithm, false)

// When true, QuicFramer will use AppendIetfConnectionIdsNew instead of
// AppendIetfConnectionId.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_use_new_append_connection_id,
          false)

// If true, QuicStreamSequencerBuffer will switch to a new
// QuicIntervalSet::AddOptimizedForAppend method in OnStreamData().
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_faster_interval_add_in_sequence_buffer,
    false)

// If true, GFE time wait list will send termination packets based on current
// packet's encryption level.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_fix_termination_packets, false)

// If true, stop using AckBundling mode to send ACK, also deprecate ack_queued
// from QuicConnection.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_deprecate_ack_bundling_mode,
          false)

// If true, enforce that QUIC CHLOs fit in one packet.
QUIC_FLAG(bool, FLAGS_quic_enforce_single_packet_chlo, true)

// If both this flag and gfe2_reloadable_flag_quic_deprecate_ack_bundling_mode
// are true, QuicReceivedPacketManager decides when to send ACKs.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_rpm_decides_when_to_send_acks,
          false)

// If true, instead of send encryption none termination packets, send stateless
// reset in reponse to short headers.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_always_reset_short_header_packets,
          false)
