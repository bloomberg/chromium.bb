// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_interfaces.h"

#include "net/android/network_library.h"

namespace net {

std::string GetWifiSSID() {
  return android::GetWifiSSID();
}

}  // namespace net
