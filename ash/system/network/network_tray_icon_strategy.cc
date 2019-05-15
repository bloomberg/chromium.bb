// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_tray_icon_strategy.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/active_network_icon.h"
#include "ash/system/network/network_icon.h"
#include "base/logging.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace tray {

namespace {

// OOBE has a white background that makes regular tray icons not visible.
network_icon::IconType GetIconType() {
  if (Shell::Get()->session_controller()->GetSessionState() ==
      session_manager::SessionState::OOBE) {
    return network_icon::ICON_TYPE_TRAY_OOBE;
  }
  return network_icon::ICON_TYPE_TRAY_REGULAR;
}

}  // namespace

gfx::ImageSkia DefaultNetworkTrayIconStrategy::GetNetworkIcon(bool* animating) {
  return Shell::Get()
      ->system_tray_model()
      ->active_network_icon()
      ->GetDualImagePrimary(GetIconType(), animating);
}

gfx::ImageSkia MobileNetworkTrayIconStrategy::GetNetworkIcon(bool* animating) {
  return Shell::Get()
      ->system_tray_model()
      ->active_network_icon()
      ->GetDualImageCellular(GetIconType(), animating);
}

gfx::ImageSkia SingleNetworkTrayIconStrategy::GetNetworkIcon(bool* animating) {
  return Shell::Get()
      ->system_tray_model()
      ->active_network_icon()
      ->GetSingleImage(GetIconType(), animating);
}

}  // namespace tray
}  // namespace ash
