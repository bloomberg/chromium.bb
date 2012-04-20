// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_NETWORK_CHANGE_NOTIFIER_FACTORY_H_
#define NET_ANDROID_NETWORK_CHANGE_NOTIFIER_FACTORY_H_
#pragma once

#include "base/compiler_specific.h"
#include "net/base/network_change_notifier_factory.h"

namespace net {

class NetworkChangeNotifier;

namespace android {

// NetworkChangeNotifierFactory creates Android-specific specialization of
// NetworkChangeNotifier.
class NetworkChangeNotifierFactory : public net::NetworkChangeNotifierFactory {
 public:
  NetworkChangeNotifierFactory();

  // Overrides of net::NetworkChangeNotifierFactory.
  virtual net::NetworkChangeNotifier* CreateInstance() OVERRIDE;
};

}  // namespace android
}  // namespace net

#endif  // NET_ANDROID_NETWORK_CHANGE_NOTIFIER_FACTORY_H_
