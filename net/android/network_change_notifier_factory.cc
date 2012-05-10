// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/network_change_notifier_factory.h"

#include "net/android/network_change_notifier_android.h"

namespace net {
namespace android {

NetworkChangeNotifierFactory::NetworkChangeNotifierFactory() {}

net::NetworkChangeNotifier* NetworkChangeNotifierFactory::CreateInstance() {
  return new NetworkChangeNotifier();
}

}  // namespace android
}  // namespace net
