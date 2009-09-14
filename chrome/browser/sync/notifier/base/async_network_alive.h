// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_BASE_ASYNC_NETWORK_ALIVE_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_BASE_ASYNC_NETWORK_ALIVE_H_

#include "talk/base/signalthread.h"

namespace notifier {

// System specific info needed for changes.
class PlatformNetworkInfo;

class AsyncNetworkAlive : public talk_base::SignalThread {
 public:
  static AsyncNetworkAlive* Create();

  virtual ~AsyncNetworkAlive() {}

  bool alive() const {
    return alive_;
  }

  bool error() const {
    return error_;
  }

  void SetWaitForNetworkChange(PlatformNetworkInfo* previous_info) {
    network_info_ = previous_info;
  }

  PlatformNetworkInfo* ReleaseInfo() {
    PlatformNetworkInfo* info = network_info_;
    network_info_ = NULL;
    return info;
  }

 protected:
  AsyncNetworkAlive() : network_info_(NULL), alive_(false), error_(false) {
  }

  PlatformNetworkInfo* network_info_;
  bool alive_;
  bool error_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AsyncNetworkAlive);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_BASE_ASYNC_NETWORK_ALIVE_H_
