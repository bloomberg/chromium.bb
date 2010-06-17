// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/network_change_notifier_proxy.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/common/net/network_change_observer_proxy.h"

namespace chrome_common_net {

NetworkChangeNotifierProxy::NetworkChangeNotifierProxy(
    NetworkChangeNotifierThread* source_thread)
    : observer_proxy_(new NetworkChangeObserverProxy(
        source_thread, MessageLoop::current())),
      observer_repeater_(&observers_) {
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
    NetworkObserverList* observers)
    : observers_(observers) {
  DCHECK(observers_);
}

NetworkChangeNotifierProxy::ObserverRepeater::~ObserverRepeater() {
  DCHECK(CalledOnValidThread());
}

void NetworkChangeNotifierProxy::ObserverRepeater::OnIPAddressChanged() {
  DCHECK(CalledOnValidThread());
  FOR_EACH_OBSERVER(net::NetworkChangeNotifier::Observer, *observers_,
                    OnIPAddressChanged());
}

}  // namespace chrome_common_net
