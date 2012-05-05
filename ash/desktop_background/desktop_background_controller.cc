// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_controller.h"

#include "ash/desktop_background/desktop_background_view.h"
#include "ash/shell.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/root_window_layout_manager.h"
#include "base/logging.h"
#include "grit/ui_resources.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/widget.h"

namespace ash {

DesktopBackgroundController::DesktopBackgroundController() :
  desktop_background_mode_(BACKGROUND_IMAGE) {
}

DesktopBackgroundController::~DesktopBackgroundController() {
}

void DesktopBackgroundController::SetDesktopBackgroundImageMode() {
  internal::RootWindowLayoutManager* root_window_layout =
      Shell::GetInstance()->root_window_layout();
  int index = Shell::GetInstance()->user_wallpaper_delegate()->
      GetUserWallpaperIndex();
  // We should not change background when index is invalid. For instance, at
  // login screen.
  if (index == ash::GetInvalidWallpaperIndex())
    return;
  root_window_layout->SetBackgroundLayer(NULL);
  internal::CreateDesktopBackground(GetWallpaper(index),
                                    GetWallpaperInfo(index).layout);
  desktop_background_mode_ = BACKGROUND_IMAGE;
}

void DesktopBackgroundController::SetDesktopBackgroundSolidColorMode() {
  // Set a solid black background.
  // TODO(derat): Remove this in favor of having the compositor only clear the
  // viewport when there are regions not covered by a layer:
  // http://crbug.com/113445
  Shell* shell = Shell::GetInstance();
  ui::Layer* background_layer = new ui::Layer(ui::LAYER_SOLID_COLOR);
  background_layer->SetColor(SK_ColorBLACK);
  shell->GetContainer(internal::kShellWindowId_DesktopBackgroundContainer)->
      layer()->Add(background_layer);
  shell->root_window_layout()->SetBackgroundLayer(background_layer);
  shell->root_window_layout()->SetBackgroundWidget(NULL);
  desktop_background_mode_ = BACKGROUND_SOLID_COLOR;
}

}  // namespace ash
