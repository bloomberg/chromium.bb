// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/auto_reload_bridge.h"

#include <memory>

#include "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"
#include "net/base/network_change_notifier.h"

namespace {
class NetworkChangeObserverBridge;
}  // namespace

@interface AutoReloadBridge () {
  base::WeakNSObject<Tab> _tab;
  base::scoped_nsobject<AutoReloadController> _controller;
  std::unique_ptr<NetworkChangeObserverBridge> _networkBridge;
}

// This method is called by NetworkChangeObserverBridge to indicate that the
// device's connected state changed to |online|.
- (void)networkStateChangedToOnline:(BOOL)online;

@end

namespace {

class NetworkChangeObserverBridge
    : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  explicit NetworkChangeObserverBridge(AutoReloadBridge* bridge)
      : bridge_(bridge) {}
  ~NetworkChangeObserverBridge() override {}

  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    bool online = type == net::NetworkChangeNotifier::CONNECTION_NONE;
    [bridge_ networkStateChangedToOnline:online];
  }

 private:
  AutoReloadBridge* bridge_;
};

}  // namespace

@implementation AutoReloadBridge

- (instancetype)initWithTab:(Tab*)tab {
  DCHECK(tab);
  if ((self = [super init])) {
    BOOL online = !net::NetworkChangeNotifier::IsOffline();
    _tab.reset(tab);
    _controller.reset([[AutoReloadController alloc] initWithDelegate:self
                                                        onlineStatus:online]);
    _networkBridge.reset(new NetworkChangeObserverBridge(self));
  }
  return self;
}

- (void)loadStartedForURL:(const GURL&)url {
  [_controller loadStartedForURL:url];
}

- (void)loadFinishedForURL:(const GURL&)url wasPost:(BOOL)wasPost {
  [_controller loadFinishedForURL:url wasPost:wasPost];
}

- (void)loadFailedForURL:(const GURL&)url wasPost:(BOOL)wasPost {
  [_controller loadFailedForURL:url wasPost:wasPost];
}

- (void)networkStateChangedToOnline:(BOOL)online {
  [_controller networkStateChangedToOnline:online];
}

#pragma mark AutoReloadDelegate methods

- (void)reload {
  [_tab reload];
}

@end
