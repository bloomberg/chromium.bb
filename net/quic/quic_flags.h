// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_FLAGS_H_
#define NET_QUIC_QUIC_FLAGS_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"

NET_EXPORT_PRIVATE extern bool FLAGS_quic_allow_oversized_packets_for_test;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_use_time_loss_detection;
NET_EXPORT_PRIVATE extern bool FLAGS_use_early_return_when_verifying_chlo;
NET_EXPORT_PRIVATE extern bool FLAGS_enable_quic_fec;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_use_bbr_congestion_control;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_allow_bbr;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_too_many_outstanding_packets;
NET_EXPORT_PRIVATE extern int64 FLAGS_quic_time_wait_list_seconds;
NET_EXPORT_PRIVATE extern int64 FLAGS_quic_time_wait_list_max_connections;
NET_EXPORT_PRIVATE extern bool FLAGS_enable_quic_stateless_reject_support;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_auto_tune_receive_window;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_do_path_mtu_discovery;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_process_frames_inline;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_dont_write_when_flow_unblocked;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_allow_ip_migration;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_use_conservative_receive_buffer;
NET_EXPORT_PRIVATE extern bool FLAGS_increase_time_wait_list;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_limit_max_cwnd;
NET_EXPORT_PRIVATE extern bool FLAGS_spdy_strip_invalid_headers;
NET_EXPORT_PRIVATE extern bool FLAGS_exact_stream_id_delta;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_limit_pacing_burst;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_require_handshake_confirmation;

#endif  // NET_QUIC_QUIC_FLAGS_H_
