// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_controller.h"

#include "ash/desktop_background/desktop_background_view.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
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
#include "ui/gfx/rect.h"
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

// Stores the current wallpaper data.
struct DesktopBackgroundController::WallpaperData {
  explicit WallpaperData(int index)
      : wallpaper_index(index),
        wallpaper_layout(GetWallpaperInfo(index).layout),
        wallpaper_image(*(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            GetWallpaperInfo(index).id).ToImageSkia())) {
  }
  WallpaperData(WallpaperLayout layout, const gfx::ImageSkia& image)
      : wallpaper_index(-1),
        wallpaper_layout(layout),
        wallpaper_image(image) {
  }
  const int wallpaper_index;
  const WallpaperLayout wallpaper_layout;
  const gfx::ImageSkia wallpaper_image;
};

// DesktopBackgroundController::WallpaperOperation wraps background wallpaper
// loading.
class DesktopBackgroundController::WallpaperOperation
    : public base::RefCountedThreadSafe<
          DesktopBackgroundController::WallpaperOperation> {
 public:
  explicit WallpaperOperation(int index) : index_(index) {
  }

  static void Run(scoped_refptr<WallpaperOperation> wo) {
    wo->LoadingWallpaper();
  }

  void LoadingWallpaper() {
    if (cancel_flag_.IsSet())
      return;
    wallpaper_data_.reset(new WallpaperData(index_));
  }

  void Cancel() {
    cancel_flag_.Set();
  }

  WallpaperData* ReleaseWallpaperData() {
    return wallpaper_data_.release();
  }

 private:
  friend class base::RefCountedThreadSafe<
      DesktopBackgroundController::WallpaperOperation>;

  ~WallpaperOperation() {}

  base::CancellationFlag cancel_flag_;

  scoped_ptr<WallpaperData> wallpaper_data_;

  int index_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperOperation);
};

DesktopBackgroundController::DesktopBackgroundController()
    : locked_(false),
      desktop_background_mode_(BACKGROUND_SOLID_COLOR),
      background_color_(SK_ColorGRAY),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  InstallComponentForAllWindows();
}

DesktopBackgroundController::~DesktopBackgroundController() {
  CancelPendingWallpaperOperation();
}

gfx::ImageSkia DesktopBackgroundController::GetWallpaper() const {
  if (current_wallpaper_.get())
    return current_wallpaper_->wallpaper_image;
  return gfx::ImageSkia();
}

WallpaperLayout DesktopBackgroundController::GetWallpaperLayout() const {
  if (current_wallpaper_.get())
    return current_wallpaper_->wallpaper_layout;
  return CENTER_CROPPED;
}

SkBitmap DesktopBackgroundController::GetCurrentWallpaperImage() {
  if (desktop_background_mode_ != BACKGROUND_IMAGE)
    return SkBitmap();
  return GetWallpaper();
}

void DesktopBackgroundController::OnRootWindowAdded(
    aura::RootWindow* root_window) {
  InstallComponent(root_window);
}

void DesktopBackgroundController::SetDefaultWallpaper(int index) {
  // We should not change background when index is invalid. For instance, at
  // login screen or stub_user login.
  if (index == GetInvalidWallpaperIndex()) {
    CreateEmptyWallpaper();
    return;
  } else if (index == GetSolidColorIndex()) {
    SetDesktopBackgroundSolidColorMode(kLoginWallpaperColor);
    return;
  }

  if (current_wallpaper_.get() && current_wallpaper_->wallpaper_index == index)
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
  CancelPendingWallpaperOperation();
  current_wallpaper_.reset(new WallpaperData(layout, wallpaper));
  SetDesktopBackgroundImageMode();
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
  background_color_ = color;
  if (desktop_background_mode_ != BACKGROUND_SOLID_COLOR) {
    desktop_background_mode_ = BACKGROUND_SOLID_COLOR;
    InstallComponentForAllWindows();
    return;
  }

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    aura::RootWindow* root_window = *iter;
    internal::DesktopBackgroundWidgetController* component = root_window->
        GetProperty(internal::kWindowDesktopComponent);
    DCHECK(component);
    DCHECK(component->layer());
    component->layer()->SetColor(background_color_ );
  }
}

void DesktopBackgroundController::CreateEmptyWallpaper() {
  current_wallpaper_.reset(NULL);
  SetDesktopBackgroundImageMode();
}

void DesktopBackgroundController::MoveDesktopToLockedContainer() {
  if (locked_)
    return;
  locked_ = true;
  ReparentBackgroundWidgets(GetBackgroundContainerId(false),
                            GetBackgroundContainerId(true));
}

void DesktopBackgroundController::MoveDesktopToUnlockedContainer() {
  if (!locked_)
    return;
  locked_ = false;
  ReparentBackgroundWidgets(GetBackgroundContainerId(true),
                            GetBackgroundContainerId(false));
}

void DesktopBackgroundController::SetDesktopBackgroundImageMode() {
  if (desktop_background_mode_ != BACKGROUND_IMAGE) {
    desktop_background_mode_ = BACKGROUND_IMAGE;
    InstallComponentForAllWindows();
    return;
  }

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::iterator iter = root_windows.begin();
      iter != root_windows.end(); ++iter) {
    aura::RootWindow* root_window = *iter;
    internal::DesktopBackgroundWidgetController* component = root_window->
        GetProperty(internal::kWindowDesktopComponent);
    DCHECK(component);
    DCHECK(component->widget());
    aura::Window* window = component->widget()->GetNativeView();
    gfx::Rect bounds = window->bounds();
    window->SchedulePaintInRect(gfx::Rect(0, 0,
                                          bounds.width(), bounds.height()));
  }
}

void DesktopBackgroundController::OnWallpaperLoadCompleted(
    scoped_refptr<WallpaperOperation> wo) {
  current_wallpaper_.reset(wo->ReleaseWallpaperData());

  SetDesktopBackgroundImageMode();

  DCHECK(wo.get() == wallpaper_op_.get());
  wallpaper_op_ = NULL;
}

ui::Layer* DesktopBackgroundController::SetColorLayerForContainer(
    SkColor color,
    aura::RootWindow* root_window,
    int container_id) {
  ui::Layer* background_layer = new ui::Layer(ui::LAYER_SOLID_COLOR);
  background_layer->SetColor(color);

  Shell::GetContainer(root_window,container_id)->
      layer()->Add(background_layer);
  return background_layer;
}

void DesktopBackgroundController::InstallComponent(
    aura::RootWindow* root_window) {
  internal::DesktopBackgroundWidgetController* component = NULL;
  int container_id = GetBackgroundContainerId(locked_);

  switch (desktop_background_mode_) {
    case BACKGROUND_IMAGE: {
      views::Widget* widget = internal::CreateDesktopBackground(root_window,
                                                                container_id);
      component = new internal::DesktopBackgroundWidgetController(widget);
      break;
    }
    case BACKGROUND_SOLID_COLOR: {
      ui::Layer* layer = SetColorLayerForContainer(background_color_,
                                                   root_window,
                                                   container_id);
      component = new internal::DesktopBackgroundWidgetController(layer);
      break;
    }
    default: {
      NOTREACHED();
    }
  }
  root_window->SetProperty(internal::kWindowDesktopComponent, component);
}

void DesktopBackgroundController::InstallComponentForAllWindows() {
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    InstallComponent(*iter);
  }
}

void DesktopBackgroundController::ReparentBackgroundWidgets(int src_container,
                                                            int dst_container) {
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::iterator iter = root_windows.begin();
    iter != root_windows.end(); ++iter) {
    aura::RootWindow* root_window = *iter;
    internal::DesktopBackgroundWidgetController* component = root_window->
        GetProperty(internal::kWindowDesktopComponent);
    DCHECK(component);
    component->Reparent(root_window,
                        src_container,
                        dst_container);
  }
}

int DesktopBackgroundController::GetBackgroundContainerId(bool locked) {
  return locked ? internal::kShellWindowId_LockScreenBackgroundContainer :
                  internal::kShellWindowId_DesktopBackgroundContainer;
}

}  // namespace ash
