// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/base/network_status_detector_task.h"

namespace notifier {

void NetworkStatusDetectorTask::DetectNetworkState() {
  // If the detection has been finished, then just broadcast the current
  // state. Otherwise, allow the signal to be sent when the initial
  // detection is finished.
  if (initial_detection_done_) {
    SignalNetworkStateDetected(is_alive_, is_alive_);
  }
}

void NetworkStatusDetectorTask::SetNetworkAlive(bool is_alive) {
  bool was_alive = is_alive_;
  is_alive_ = is_alive;

  if (!initial_detection_done_ || was_alive != is_alive_) {
    initial_detection_done_ = true;

    // Tell everyone about the network state change.
    SignalNetworkStateDetected(was_alive, is_alive_);
  }
}

}  // namespace notifier
