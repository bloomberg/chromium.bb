// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/status_icons/status_tray_mac.h"

#include "chrome/browser/ui/cocoa/status_icons/status_icon_mac.h"

StatusTray* StatusTray::Create() {
  return new StatusTrayMac();
}

StatusTrayMac::StatusTrayMac() {
}

StatusIcon* StatusTrayMac::CreatePlatformStatusIcon() {
  return new StatusIconMac();
}
