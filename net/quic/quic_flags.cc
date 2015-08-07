// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_flags.h"

bool FLAGS_quic_allow_oversized_packets_for_test = false;

// When true, the use time based loss detection instead of nack.
bool FLAGS_quic_use_time_loss_detection = false;

// If true, it will return as soon as an error is detected while validating
// CHLO.
bool FLAGS_use_early_return_when_verifying_chlo = true;

// If true, QUIC connections will support FEC protection of data while sending
// packets, to reduce latency of data delivery to the application. The client
// must also request FEC protection for the server to use FEC.
bool FLAGS_enable_quic_fec = false;

// When true, defaults to BBR congestion control instead of Cubic.
bool FLAGS_quic_use_bbr_congestion_control = false;

// If true, QUIC BBR congestion control may be enabled via Finch and/or via QUIC
// connection options.
bool FLAGS_quic_allow_bbr = false;

// Time period for which a given connection_id should live in the time-wait
// state.
int64 FLAGS_quic_time_wait_list_seconds = 200;

// Currently, this number is quite conservative.  The max QPS limit for an
// individual server silo is currently set to 1000 qps, though the actual max
// that we see in the wild is closer to 450 qps.  Regardless, this means that
// the longest time-wait list we should see is 200 seconds * 1000 qps = 200000.
// Of course, there are usually many queries per QUIC connection, so we allow a
// factor of 3 leeway.
//
// Maximum number of connections on the time-wait list. A negative value implies
// no configured limit.
int64 FLAGS_quic_time_wait_list_max_connections = 600000;

// Enables server-side support for QUIC stateless rejects.
bool FLAGS_enable_quic_stateless_reject_support = true;

// If true, flow controller may grow the receive window size if necessary.
bool FLAGS_quic_auto_tune_receive_window = true;

// Enables server-side path MTU discovery in QUIC.
bool FLAGS_quic_do_path_mtu_discovery = true;

// Process QUIC frames as soon as they're parsed, instead of waiting for the
// packet's parsing to complete.
bool FLAGS_quic_process_frames_inline = true;

// No longer call OnCanWrite when connection level flow control unblocks in
// QuicSession.
bool FLAGS_quic_dont_write_when_flow_unblocked = true;

// If true, client IP migration is allowed in QUIC.
bool FLAGS_quic_allow_ip_migration = true;

// Estimate that only 60% of QUIC's receive buffer is usable as opposed to 95%.
bool FLAGS_quic_use_conservative_receive_buffer = true;

// If true, default quic_time_wait_list_seconds (time to keep a connection ID on
// the time-wait list) is 200 seconds rather than 5 seconds and increase the
// maximum time-wait list size to 600,000.
bool FLAGS_increase_time_wait_list = true;

// Limits QUIC's max CWND to 200 packets.
bool FLAGS_quic_limit_max_cwnd = true;

// If true, don't serialize invalid HTTP headers when converting HTTP to SPDY.
bool FLAGS_spdy_strip_invalid_headers = true;

// If true, instead of enforcing a fixed limit of 200 between the last
// client-created stream ID and the next one, calculate the difference based on
// get_max_open_streams().
bool FLAGS_exact_stream_id_delta = true;

// Limits the pacing burst out of quiescence to the current congestion window in
// packets.
bool FLAGS_quic_limit_pacing_burst = true;

// If true, require handshake confirmation for QUIC connections, functionally
// disabling 0-rtt handshakes.
// TODO(rtenneti): Enable this flag after fixing tests.
bool FLAGS_quic_require_handshake_confirmation = false;
