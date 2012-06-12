// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_controller.h"

#include "ash/desktop_background/desktop_background_view.h"
#include "ash/shell.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/root_window_layout_manager.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/threading/worker_pool.h"
#include "grit/ui_resources.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {
internal::RootWindowLayoutManager* GetRootWindowLayoutManager(
    aura::RootWindow* root_window) {
  return static_cast<internal::RootWindowLayoutManager*>(
      root_window->layout_manager());
}
}  // namespace

// DesktopBackgroundController::WallpaperOperation wraps background wallpaper
// loading.
class DesktopBackgroundController::WallpaperOperation
    : public base::RefCountedThreadSafe<
    DesktopBackgroundController::WallpaperOperation> {
 public:
  WallpaperOperation(int index)
      : wallpaper_(NULL),
        layout_(CENTER_CROPPED),
        index_(index) {
  }

  static void Run(scoped_refptr<WallpaperOperation> wo) {
    wo->LoadingWallpaper();
  }

  void LoadingWallpaper() {
    if (cancel_flag_.IsSet())
      return;
    wallpaper_ = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      GetWallpaperInfo(index_).id).ToImageSkia();
    if (cancel_flag_.IsSet())
      return;
    layout_ = GetWallpaperInfo(index_).layout;
  }

  void Cancel() {
    cancel_flag_.Set();
  }

  const gfx::ImageSkia* wallpaper() {
    return wallpaper_;
  }

  WallpaperLayout wallpaper_layout() {
    return layout_;
  }

  int index() {
    return index_;
  }

 private:
  friend class base::RefCountedThreadSafe<
      DesktopBackgroundController::WallpaperOperation>;

  base::CancellationFlag cancel_flag_;

  const gfx::ImageSkia* wallpaper_;
  WallpaperLayout layout_;
  int index_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperOperation);
};

DesktopBackgroundController::DesktopBackgroundController(
    aura::RootWindow* root_window)
    : root_window_(root_window),
      desktop_background_mode_(BACKGROUND_IMAGE),
      previous_index_(-1),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(root_window_);
}

DesktopBackgroundController::~DesktopBackgroundController() {
  CancelPendingWallpaperOperation();
}

void DesktopBackgroundController::SetDefaultWallpaper(int index) {
  // We should not change background when index is invalid. For instance, at
  // login screen or stub_user login.
  if (index == ash::GetInvalidWallpaperIndex()) {
    CreateEmptyWallpaper();
    return;
  } else if (index == ash::GetSolidColorIndex()) {
    SetDesktopBackgroundSolidColorMode(kLoginWallpaperColor);
    return;
  }

  if (previous_index_ == index)
    return;

  CancelPendingWallpaperOperation();

  wallpaper_op_ = new WallpaperOperation(index);
  base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WallpaperOperation::Run, wallpaper_op_),
      base::Bind(&DesktopBackgroundController::OnWallpaperLoadCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 wallpaper_op_),
      true /* task_is_slow */);
}

void DesktopBackgroundController::SetCustomWallpaper(
    const gfx::ImageSkia& wallpaper,
    WallpaperLayout layout) {
  GetRootWindowLayoutManager(root_window_)->SetBackgroundLayer(NULL);
  internal::CreateDesktopBackground(wallpaper, layout, root_window_);
  desktop_background_mode_ = BACKGROUND_IMAGE;
}

void DesktopBackgroundController::CancelPendingWallpaperOperation() {
  // Set canceled flag of previous request to skip unneeded loading.
  if (wallpaper_op_.get())
    wallpaper_op_->Cancel();

  // Cancel reply callback for previous request.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void DesktopBackgroundController::SetDesktopBackgroundSolidColorMode(
    SkColor color) {
  // Set a solid black background.
  // TODO(derat): Remove this in favor of having the compositor only clear the
  // viewport when there are regions not covered by a layer:
  // http://crbug.com/113445
  ui::Layer* background_layer = new ui::Layer(ui::LAYER_SOLID_COLOR);
  background_layer->SetColor(color);
  root_window_->GetChildById(
      internal::kShellWindowId_DesktopBackgroundContainer)->
      layer()->Add(background_layer);
  GetRootWindowLayoutManager(root_window_)->SetBackgroundLayer(
      background_layer);
  GetRootWindowLayoutManager(root_window_)->SetBackgroundWidget(NULL);
  desktop_background_mode_ = BACKGROUND_SOLID_COLOR;
}

void DesktopBackgroundController::SetDesktopBackgroundImageMode(
    scoped_refptr<WallpaperOperation> wo) {
  GetRootWindowLayoutManager(root_window_)->SetBackgroundLayer(NULL);
  if(wo->wallpaper()) {
    internal::CreateDesktopBackground(
        *wo->wallpaper(), wo->wallpaper_layout(), root_window_);
    desktop_background_mode_ = BACKGROUND_IMAGE;
  }
}

void DesktopBackgroundController::OnWallpaperLoadCompleted(
    scoped_refptr<WallpaperOperation> wo) {
  SetDesktopBackgroundImageMode(wo);
  previous_index_ = wo->index();

  DCHECK(wo.get() == wallpaper_op_.get());
  wallpaper_op_ = NULL;
}

void DesktopBackgroundController::CreateEmptyWallpaper() {
  gfx::ImageSkia dummy;
  internal::CreateDesktopBackground(dummy, CENTER, root_window_);
  desktop_background_mode_ = BACKGROUND_IMAGE;
}

}  // namespace ash
