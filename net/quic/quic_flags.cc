// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_flags.h"

// TODO(rtenneti): Remove this.
// Do not flip this flag until the flakiness of the
// net/tools/quic/end_to_end_test is fixed.
// If true, then QUIC connections will track the retransmission history of a
// packet so that an ack of a previous transmission will ack the data of all
// other transmissions.
bool FLAGS_track_retransmission_history = false;

// Do not remove this flag until the Finch-trials described in b/11706275
// are complete.
// If true, QUIC connections will support the use of a pacing algorithm when
// sending packets, in an attempt to reduce packet loss.  The client must also
// request pacing for the server to enable it.
bool FLAGS_enable_quic_pacing = true;

// Do not remove this flag until b/11792453 is marked as Fixed.
// If true, turns on stream flow control in QUIC.
bool FLAGS_enable_quic_stream_flow_control_2 = true;

bool FLAGS_quic_allow_oversized_packets_for_test = false;
bool FLAGS_quic_congestion_control_inter_arrival = false;
// When true, the use time based loss detection instead of nack.
bool FLAGS_quic_use_time_loss_detection = false;
