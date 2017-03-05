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

views::View* TrayTiles::GetHelpButtonView() const {
  if (!default_view_)
    return nullptr;
  return default_view_->GetHelpButtonView();
}

TilesDefaultView* TrayTiles::GetDefaultViewForTesting() const {
  return default_view_;
}

views::View* TrayTiles::CreateDefaultViewForTesting(LoginStatus status) {
  return CreateDefaultView(status);
}

views::View* TrayTiles::CreateDefaultView(LoginStatus status) {
  CHECK(default_view_ == nullptr);
  default_view_ = new TilesDefaultView(this, status);
  default_view_->Init();
  return default_view_;
}

void TrayTiles::DestroyDefaultView() {
  default_view_ = nullptr;
}

}  // namespace ash
