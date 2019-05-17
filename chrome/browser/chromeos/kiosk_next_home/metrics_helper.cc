// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_next_home/metrics_helper.h"

#include "base/metrics/histogram_macros.h"

namespace chromeos {
namespace kiosk_next_home {

void RecordBridgeAction(BridgeAction action) {
  UMA_HISTOGRAM_ENUMERATION("KioskNextHome.Bridge.Action", action);
}

}  // namespace kiosk_next_home
}  // namespace chromeos
