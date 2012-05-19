// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_tray.h"

#include <algorithm>

#include "chrome/browser/status_icons/status_icon.h"

StatusTray::~StatusTray() {
}

StatusIcon* StatusTray::CreateStatusIcon() {
  StatusIcon* icon = CreatePlatformStatusIcon();
  if (icon)
    status_icons_.push_back(icon);
  return icon;
}

void StatusTray::RemoveStatusIcon(StatusIcon* icon) {
  StatusIcons::iterator i(
      std::find(status_icons_.begin(), status_icons_.end(), icon));

  if (i == status_icons_.end()) {
    NOTREACHED();
    return;
  }

  status_icons_.erase(i);
}

StatusTray::StatusTray() {
}
