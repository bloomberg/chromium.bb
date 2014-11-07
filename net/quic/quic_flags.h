// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_FLAGS_H_
#define NET_QUIC_QUIC_FLAGS_H_

#include "net/base/net_export.h"

NET_EXPORT_PRIVATE extern bool FLAGS_track_retransmission_history;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_allow_oversized_packets_for_test;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_use_time_loss_detection;
NET_EXPORT_PRIVATE extern bool FLAGS_use_early_return_when_verifying_chlo;
NET_EXPORT_PRIVATE extern bool FLAGS_send_quic_crypto_reject_reason;
NET_EXPORT_PRIVATE extern bool FLAGS_enable_quic_fec;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_use_bbr_congestion_control;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_allow_more_open_streams;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_unified_timeouts;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_drop_junk_packets;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_allow_bbr;
NET_EXPORT_PRIVATE extern bool FLAGS_allow_truncated_connection_ids_for_quic;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_too_many_outstanding_packets;
NET_EXPORT_PRIVATE extern bool FLAGS_enable_quic_delay_forward_security;

#endif  // NET_QUIC_QUIC_FLAGS_H_
