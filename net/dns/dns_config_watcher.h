// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_WATCHER_H_
#define NET_DNS_DNS_CONFIG_WATCHER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace net {
namespace internal {

// Watches when the system DNS settings have changed. It is used by
// NetworkChangeNotifier to provide signals to registered DNSObservers.
// Depending on the platform, watches files, Windows registry, or libnotify key.
// If some watches fail, we keep the working parts, but NetworkChangeNotifier
// will mark notifications to DNSObserver with the CHANGE_DNS_WATCH_FAILED bit.
class DnsConfigWatcher {
 public:
  DnsConfigWatcher();
  ~DnsConfigWatcher();

  // Starts watching system configuration for changes. The current thread must
  // have a MessageLoopForIO. The signals will be delivered directly to
  // the global NetworkChangeNotifier.
  void Init();

  // Must be called on the same thread as Init. Required if dtor will be called
  // on a different thread.
  void CleanUp();

 private:
  // Platform-specific implementation.
  class Core;
  scoped_ptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(DnsConfigWatcher);
};

}  // namespace internal
}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_WATCHER_H_
