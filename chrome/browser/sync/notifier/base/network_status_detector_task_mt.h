// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_BASE_NETWORK_STATUS_DETECTOR_TASK_MT_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_BASE_NETWORK_STATUS_DETECTOR_TASK_MT_H_

#include "chrome/browser/sync/notifier/base/network_status_detector_task.h"

namespace notifier {

class AsyncNetworkAlive;
class PlatformNetworkInfo;

class NetworkStatusDetectorTaskMT : public NetworkStatusDetectorTask {
 public:
  explicit NetworkStatusDetectorTaskMT(talk_base::Task* parent)
      : NetworkStatusDetectorTask(parent) {
  }

 protected:
  virtual int ProcessStart() {
    StartAsyncDetection(NULL);
    return STATE_RESPONSE;
  }

 private:
  void OnNetworkAliveDone(AsyncNetworkAlive* network_alive);
  void StartAsyncDetection(PlatformNetworkInfo* network_info);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_BASE_NETWORK_STATUS_DETECTOR_TASK_MT_H_
