// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_types.h"

namespace ash {

const int kLauncherPreferredSize = 48;
const int kLauncherBackgroundAlpha = 204;
const int kInvalidImageResourceID = -1;
const int kInvalidLauncherID = 0;
const int kTimeToSwitchBackgroundMs = 1000;

LauncherItem::LauncherItem()
    : type(TYPE_UNDEFINED),
      id(kInvalidLauncherID),
      status(STATUS_CLOSED) {
}

LauncherItem::~LauncherItem() {
}

LauncherItemDetails::LauncherItemDetails()
    : type(TYPE_UNDEFINED),
      image_resource_id(kInvalidImageResourceID) {
}

LauncherItemDetails::~LauncherItemDetails() {
}

}  // namespace ash
