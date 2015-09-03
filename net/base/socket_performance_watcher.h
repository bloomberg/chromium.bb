// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SOCKET_PERFORMANCE_WATCHER_H_
#define NET_BASE_SOCKET_PERFORMANCE_WATCHER_H_

#include "base/macros.h"
#include "net/base/net_export.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace net {

// SocketPerformanceWatcher is the base class for recording and aggregating
// socket statistics.
class NET_EXPORT_PRIVATE SocketPerformanceWatcher {
 public:
  virtual ~SocketPerformanceWatcher() {}

  // Called when updated transport layer RTT information is available. This
  // must be the transport layer RTT from this device to the remote transport
  // layer endpoint. If the request goes through a HTTP proxy, it should
  // provide the RTT to the proxy.
  virtual void OnUpdatedRTTAvailable(const base::TimeDelta& rtt) = 0;

 protected:
  SocketPerformanceWatcher() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SocketPerformanceWatcher);
};

}  // namespace net

#endif  // NET_BASE_SOCKET_PERFORMANCE_WATCHER_H_
