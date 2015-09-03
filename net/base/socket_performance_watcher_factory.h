// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SOCKET_PERFORMANCE_WATCHER_FACTORY_H_
#define NET_BASE_SOCKET_PERFORMANCE_WATCHER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"

namespace net {

class SocketRecorder;

// SocketPerformanceWatcherFactory creates socket performance watcher for
// different type of sockets.
class NET_EXPORT_PRIVATE SocketPerformanceWatcherFactory {
 public:
  virtual ~SocketPerformanceWatcherFactory() {}

  // Creates a socket performance watcher that will record statistics for a
  // single TCP socket. Implementations must return a valid, unique
  // SocketRecorder for every call; recorders must not be shared across calls
  // or objects, nor is nullptr valid.
  virtual scoped_ptr<SocketPerformanceWatcher>
  CreateTCPSocketPerformanceWatcher() const = 0;

  // Creates a socket performance watcher that will record statistics for a
  // single UDP socket. Implementations must return a valid, unique
  // SocketRecorder for every call; recorders must not be shared across calls
  // or objects, nor is nullptr valid.
  virtual scoped_ptr<SocketPerformanceWatcher>
  CreateUDPSocketPerformanceWatcher() const = 0;

 protected:
  SocketPerformanceWatcherFactory() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SocketPerformanceWatcherFactory);
};

}  // namespace net

#endif  // NET_BASE_SOCKET_PERFORMANCE_WATCHER_FACTORY_H_
