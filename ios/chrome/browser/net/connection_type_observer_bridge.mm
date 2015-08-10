// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/net/connection_type_observer_bridge.h"

#include "base/logging.h"

ConnectionTypeObserverBridge::ConnectionTypeObserverBridge(
    id<CRConnectionTypeObserverBridge> delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

ConnectionTypeObserverBridge::~ConnectionTypeObserverBridge() {
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void ConnectionTypeObserverBridge::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  [delegate_ connectionTypeChanged:type];
}
