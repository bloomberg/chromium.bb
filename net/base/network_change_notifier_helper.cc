// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_helper.h"
#include <algorithm>
#include "base/logging.h"

namespace net {

namespace internal {

NetworkChangeNotifierHelper::NetworkChangeNotifierHelper()
    : is_notifying_observers_(false) {}

NetworkChangeNotifierHelper::~NetworkChangeNotifierHelper() {
  // TODO(willchan): Re-enable this DCHECK after fixing http://crbug.com/34391
  // since we're leaking URLRequestContextGetters that cause this DCHECK to
  // fire.
  // DCHECK(observers_.empty());
}

void NetworkChangeNotifierHelper::AddObserver(Observer* observer) {
  DCHECK(!is_notifying_observers_);
  observers_.push_back(observer);
}

void NetworkChangeNotifierHelper::RemoveObserver(Observer* observer) {
  DCHECK(!is_notifying_observers_);
  observers_.erase(std::remove(observers_.begin(), observers_.end(), observer));
}

void NetworkChangeNotifierHelper::OnIPAddressChanged() {
  DCHECK(!is_notifying_observers_);
  is_notifying_observers_ = true;
  for (std::vector<Observer*>::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    (*it)->OnIPAddressChanged();
  }
  is_notifying_observers_ = false;
}

}  // namespace internal

}  // namespace net
