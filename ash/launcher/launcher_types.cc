// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_types.h"

namespace ash {

const int kLauncherPreferredSize = 48;

LauncherItem::LauncherItem()
    : type(TYPE_TABBED),
      is_incognito(false),
      id(0),
      status(STATUS_CLOSED) {
}

LauncherItem::~LauncherItem() {
}

}  // namespace ash
