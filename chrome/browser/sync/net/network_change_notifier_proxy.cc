// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/net/network_change_notifier_proxy.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/net/network_change_observer_proxy.h"

namespace browser_sync {

NetworkChangeNotifierProxy::NetworkChangeNotifierProxy(
    NetworkChangeNotifierThread* source_thread)
    : observer_proxy_(new NetworkChangeObserverProxy(
        source_thread, MessageLoop::current())),
      observer_repeater_(&observers_) {
  // TODO(akalin): We get this from NonThreadSafe, which
  // net::NetworkChangeNotifier inherits from.  Interface classes
  // really shouldn't be inheriting from NonThreadSafe; make it so
  // that all the implementations of net::NetworkChangeNotifier
  // inherit from NonThreadSafe directly and
  // net::NetworkChangeNotifier doesn't.
  DCHECK(CalledOnValidThread());
  DCHECK(observer_proxy_);
  observer_proxy_->Attach(&observer_repeater_);
}

NetworkChangeNotifierProxy::~NetworkChangeNotifierProxy() {
  DCHECK(CalledOnValidThread());
  observer_proxy_->Detach();
}

void NetworkChangeNotifierProxy::AddObserver(
    net::NetworkChangeNotifier::Observer* observer) {
  DCHECK(CalledOnValidThread());
  observers_.AddObserver(observer);
}

void NetworkChangeNotifierProxy::RemoveObserver(
    net::NetworkChangeNotifier::Observer* observer) {
  DCHECK(CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

NetworkChangeNotifierProxy::ObserverRepeater::ObserverRepeater(
    NetworkObserverList* observers) : observers_(observers) {
  DCHECK(observers_);
}

NetworkChangeNotifierProxy::ObserverRepeater::~ObserverRepeater() {}

void NetworkChangeNotifierProxy::ObserverRepeater::OnIPAddressChanged() {
  DCHECK(CalledOnValidThread());
  FOR_EACH_OBSERVER(net::NetworkChangeNotifier::Observer,
                    *observers_, OnIPAddressChanged());
}

}  // namespace browser_sync
