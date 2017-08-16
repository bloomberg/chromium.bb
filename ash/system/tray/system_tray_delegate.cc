// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_delegate.h"

namespace ash {

SystemTrayDelegate::SystemTrayDelegate() = default;

SystemTrayDelegate::~SystemTrayDelegate() = default;

NetworkingConfigDelegate* SystemTrayDelegate::GetNetworkingConfigDelegate()
    const {
  return nullptr;
}

}  // namespace ash
