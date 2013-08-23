// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_OBSERVER_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_OBSERVER_H

#include <vector>

#include "base/strings/string16.h"

namespace ash {

class NetworkObserver {
 public:
  virtual ~NetworkObserver() {}

  // Called to request toggling Wi-Fi enable/disable, e.g. from an accelerator.
  // NOTE: Toggling is asynchronous and subsequent calls to query the current
  // state may return the old value.
  virtual void RequestToggleWifi() = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_OBSERVER_H
