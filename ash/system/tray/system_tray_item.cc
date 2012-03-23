// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_item.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ui/views/view.h"

namespace ash {

SystemTrayItem::SystemTrayItem() {
}

SystemTrayItem::~SystemTrayItem() {
}

void SystemTrayItem::PopupDetailedView(int for_seconds, bool activate) {
  Shell::GetInstance()->tray()->ShowDetailedView(this, for_seconds, activate);
}

void SystemTrayItem::SetDetailedViewCloseDelay(int for_seconds) {
  Shell::GetInstance()->tray()->SetDetailedViewCloseDelay(for_seconds);
}

}  // namespace ash
