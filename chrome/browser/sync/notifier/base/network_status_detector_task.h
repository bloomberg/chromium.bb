// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_BASE_NETWORK_STATUS_DETECTOR_TASK_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_BASE_NETWORK_STATUS_DETECTOR_TASK_H_

#include "chrome/browser/sync/notifier/base/time.h"
#include "talk/base/sigslot.h"
#include "talk/base/task.h"

namespace notifier {
class AsyncNetworkAlive;

// Detects the current network state and any changes to that.
class NetworkStatusDetectorTask : public talk_base::Task,
                                  public sigslot::has_slots<> {
 public:
  // Create an instance of (a subclass of) this class.
  static NetworkStatusDetectorTask* Create(talk_base::Task* parent);

  // Determines the current network state and
  // then calls SignalNetworkStateDetected.
  void DetectNetworkState();

  // Fires whenever the network state is detected.
  //   SignalNetworkStateDetected(was_alive, is_alive);
  sigslot::signal2<bool, bool> SignalNetworkStateDetected;

 protected:
  explicit NetworkStatusDetectorTask(talk_base::Task* parent)
    : talk_base::Task(parent),
      initial_detection_done_(false),
      is_alive_(false) {
  }

  virtual ~NetworkStatusDetectorTask() { }

  virtual int ProcessStart() = 0;

  // Stay around until aborted.
  virtual int ProcessResponse() {
    return STATE_BLOCKED;
  }

  void SetNetworkAlive(bool is_alive);

 private:
  bool initial_detection_done_;
  bool is_alive_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStatusDetectorTask);
};
}  // namespace notifier
#endif  // CHROME_BROWSER_SYNC_NOTIFIER_BASE_NETWORK_STATUS_DETECTOR_TASK_H_
