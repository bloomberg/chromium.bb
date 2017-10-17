// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/public/cpp/hid_usage_and_page.h"

namespace device {

bool IsProtected(const device::mojom::HidUsageAndPage& hid_usage_and_page) {
  const uint16_t usage = hid_usage_and_page.usage;
  const uint16_t usage_page = hid_usage_and_page.usage_page;

  if (usage_page == device::mojom::kPageKeyboard)
    return true;

  if (usage_page != device::mojom::kPageGenericDesktop)
    return false;

  if (usage == device::mojom::kGenericDesktopPointer ||
      usage == device::mojom::kGenericDesktopMouse ||
      usage == device::mojom::kGenericDesktopKeyboard ||
      usage == device::mojom::kGenericDesktopKeypad) {
    return true;
  }

  if (usage >= device::mojom::kGenericDesktopSystemControl &&
      usage <= device::mojom::kGenericDesktopSystemWarmRestart) {
    return true;
  }

  if (usage >= device::mojom::kGenericDesktopSystemDock &&
      usage <= device::mojom::kGenericDesktopSystemDisplaySwap) {
    return true;
  }

  return false;
}

}  // namespace device
