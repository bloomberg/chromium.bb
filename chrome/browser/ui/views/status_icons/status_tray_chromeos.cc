// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_tray_chromeos.h"

#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/views/status_icons/status_icon_chromeos.h"

StatusTray* StatusTray::Create() {
  return new StatusTrayChromeOS();
}

StatusTrayChromeOS::StatusTrayChromeOS() {
}

StatusTrayChromeOS::~StatusTrayChromeOS() {
}

StatusIcon* StatusTrayChromeOS::CreatePlatformStatusIcon() {
  return new StatusIconChromeOS();
}
