// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_WAIT_FOR_NETWORK_CALLBACK_HELPER_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_WAIT_FOR_NETWORK_CALLBACK_HELPER_H_

#include <list>

#include "base/callback.h"
#include "base/macros.h"
#include "net/base/network_change_notifier.h"

// Class used for delaying callbacks when the network connection is offline and
// invoking them when the network connection becomes online.
class WaitForNetworkCallbackHelper
    : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  WaitForNetworkCallbackHelper();
  ~WaitForNetworkCallbackHelper() override;

  // net::NetworkChangeController::NetworkChangeObserver implementation.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // If offline, saves the |callback| to be called later when online. Otherwise,
  // invokes immediately.
  void HandleCallback(base::OnceClosure callback);

 private:
  std::list<base::OnceClosure> delayed_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(WaitForNetworkCallbackHelper);
};

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_WAIT_FOR_NETWORK_CALLBACK_HELPER_H_
