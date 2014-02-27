// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/loss_detection_interface.h"

#include "net/quic/congestion_control/tcp_loss_algorithm.h"

namespace net {

// Factory for loss detection algorithm.
LossDetectionInterface* LossDetectionInterface::Create() {
  return new TCPLossAlgorithm();
}

}  // namespace net
