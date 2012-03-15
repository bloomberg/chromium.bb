// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_controller.h"

#include "ash/desktop_background/desktop_background_resources.h"
#include "ash/desktop_background/desktop_background_view.h"
#include "ash/shell.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/root_window_layout_manager.h"
#include "base/logging.h"
#include "grit/ui_resources.h"
#include "ui/aura/window.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/image/image.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/widget/widget.h"

namespace ash {

DesktopBackgroundController::DesktopBackgroundController() :
  previous_wallpaper_index_(GetDefaultWallpaperIndex()),
  desktop_background_mode_(BACKGROUND_IMAGE) {
}

DesktopBackgroundController::~DesktopBackgroundController() {
}

void DesktopBackgroundController::OnDesktopBackgroundChanged(int index) {
  internal::RootWindowLayoutManager* root_window_layout =
      Shell::GetInstance()->root_window_layout();
  if (desktop_background_mode_ == BACKGROUND_SOLID_COLOR)
    return;

  DCHECK(root_window_layout->background_widget()->widget_delegate());
  static_cast<internal::DesktopBackgroundView*>(
      root_window_layout->background_widget()->widget_delegate())->
          SetWallpaper(GetWallpaper(index));
  previous_wallpaper_index_ = index;
}

void DesktopBackgroundController::SetDesktopBackgroundImageMode(
    const SkBitmap& wallpaper) {
  internal::RootWindowLayoutManager* root_window_layout =
      Shell::GetInstance()->root_window_layout();
  root_window_layout->SetBackgroundLayer(NULL);
  root_window_layout->SetBackgroundWidget(
      internal::CreateDesktopBackground(wallpaper));
  desktop_background_mode_ = BACKGROUND_IMAGE;
}

void DesktopBackgroundController::SetDefaultDesktopBackgroundImage() {
  SetDesktopBackgroundImageMode(GetWallpaper(GetDefaultWallpaperIndex()));
}

void DesktopBackgroundController::SetPreviousDesktopBackgroundImage() {
  SetDesktopBackgroundImageMode(GetWallpaper(previous_wallpaper_index_));
}

void DesktopBackgroundController::SetDesktopBackgroundSolidColorMode() {
  // Set a solid black background.
  // TODO(derat): Remove this in favor of having the compositor only clear the
  // viewport when there are regions not covered by a layer:
  // http://crbug.com/113445
  Shell* shell = Shell::GetInstance();
  ui::Layer* background_layer = new ui::Layer(ui::Layer::LAYER_SOLID_COLOR);
  background_layer->SetColor(SK_ColorBLACK);
  shell->GetContainer(internal::kShellWindowId_DesktopBackgroundContainer)->
      layer()->Add(background_layer);
  shell->root_window_layout()->SetBackgroundLayer(background_layer);
  shell->root_window_layout()->SetBackgroundWidget(NULL);
  desktop_background_mode_ = BACKGROUND_SOLID_COLOR;
}

}  // namespace ash
