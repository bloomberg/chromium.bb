// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tiles/tray_tiles.h"

#include "ash/common/metrics/user_metrics_action.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/tiles/tiles_default_view.h"
#include "ash/common/wm_shell.h"

namespace ash {

TrayTiles::TrayTiles(SystemTray* system_tray)
    : SystemTrayItem(system_tray, UMA_NOT_RECORDED), default_view_(nullptr) {}

TrayTiles::~TrayTiles() {}

views::View* TrayTiles::CreateDefaultView(LoginStatus status) {
  WmShell* shell = WmShell::Get();
  const bool adding_user =
      shell->GetSessionStateDelegate()->IsInSecondaryLoginScreen();

  if (status == LoginStatus::LOCKED || status == LoginStatus::NOT_LOGGED_IN ||
      adding_user) {
    return nullptr;
  }

  CHECK(default_view_ == nullptr);
  default_view_ = new TilesDefaultView(this);
  default_view_->Init();

  return default_view_;
}

void TrayTiles::DestroyDefaultView() {
  default_view_ = nullptr;
}

}  // namespace ash
