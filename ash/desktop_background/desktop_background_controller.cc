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
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

const SkColor kTransparentColor = SkColorSetARGB(0x00, 0x00, 0x00, 0x00);

internal::RootWindowLayoutManager* GetRootWindowLayoutManager(
    aura::RootWindow* root_window) {
  return static_cast<internal::RootWindowLayoutManager*>(
      root_window->layout_manager());
}
}  // namespace

const int kSmallWallpaperMaxWidth = 1366;
const int kSmallWallpaperMaxHeight = 800;
const int kLargeWallpaperMaxWidth = 2560;
const int kLargeWallpaperMaxHeight = 1700;

// Stores the current wallpaper data.
struct DesktopBackgroundController::WallpaperData {
  WallpaperData(int index, WallpaperResolution resolution)
      : wallpaper_index(index),
        wallpaper_layout(GetWallpaperViewInfo(index, resolution).layout),
        wallpaper_image(*(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            GetWallpaperViewInfo(index, resolution).id).ToImageSkia())) {
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
  WallpaperOperation(int index, WallpaperResolution resolution)
      : index_(index),
        resolution_(resolution) {
  }

  static void Run(scoped_refptr<WallpaperOperation> wo) {
    wo->LoadingWallpaper();
  }

  void LoadingWallpaper() {
    if (cancel_flag_.IsSet())
      return;
    wallpaper_data_.reset(new WallpaperData(index_, resolution_));
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

  const int index_;

  const WallpaperResolution resolution_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperOperation);
};

DesktopBackgroundController::DesktopBackgroundController()
    : locked_(false),
      desktop_background_mode_(BACKGROUND_SOLID_COLOR),
      background_color_(kTransparentColor),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
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

gfx::ImageSkia DesktopBackgroundController::GetCurrentWallpaperImage() {
  if (desktop_background_mode_ != BACKGROUND_IMAGE)
    return gfx::ImageSkia();
  return GetWallpaper();
}

void DesktopBackgroundController::OnRootWindowAdded(
    aura::RootWindow* root_window) {
  // Handle resolution change for "built-in" images."
  if (BACKGROUND_IMAGE == desktop_background_mode_) {
    if (current_wallpaper_.get()) {
      gfx::Size root_window_size = root_window->GetHostSize();
      int wallpaper_width = current_wallpaper_->wallpaper_image.width();
      int wallpaper_height = current_wallpaper_->wallpaper_image.height();
      // Loads a higher resolution wallpaper if needed.
      if ((wallpaper_width < root_window_size.width() ||
           wallpaper_height < root_window_size.height()) &&
          current_wallpaper_->wallpaper_index != -1 &&
          current_wallpaper_->wallpaper_layout != TILE)
        SetDefaultWallpaper(current_wallpaper_->wallpaper_index, true);
    }
  }

  InstallComponent(root_window);
}

void DesktopBackgroundController::CacheDefaultWallpaper(int index) {
  DCHECK(index >= 0);

  WallpaperResolution resolution = GetAppropriateResolution();
  scoped_refptr<WallpaperOperation> wallpaper_op =
      new WallpaperOperation(index, resolution);
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&WallpaperOperation::Run, wallpaper_op),
      true);
}

void DesktopBackgroundController::SetDefaultWallpaper(int index,
                                                      bool force_reload) {
  // We should not change background when index is invalid. For instance, at
  // login screen or stub_user login.
  if (index == GetInvalidWallpaperIndex()) {
    CreateEmptyWallpaper();
    return;
  } else if (index == GetSolidColorIndex()) {
    SetDesktopBackgroundSolidColorMode(kLoginWallpaperColor);
    return;
  }

  if (!force_reload && current_wallpaper_.get() &&
      current_wallpaper_->wallpaper_index == index)
    return;

  CancelPendingWallpaperOperation();

  WallpaperResolution resolution = GetAppropriateResolution();

  wallpaper_op_ = new WallpaperOperation(index, resolution);
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
  desktop_background_mode_ = BACKGROUND_SOLID_COLOR;

  InstallComponentForAllWindows();
}

void DesktopBackgroundController::CreateEmptyWallpaper() {
  current_wallpaper_.reset(NULL);
  SetDesktopBackgroundImageMode();
}

WallpaperResolution DesktopBackgroundController::GetAppropriateResolution() {
  WallpaperResolution resolution = SMALL;
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    gfx::Size root_window_size = (*iter)->GetHostSize();
    if (root_window_size.width() > kSmallWallpaperMaxWidth ||
        root_window_size.height() > kSmallWallpaperMaxHeight)
      resolution = LARGE;
  }
  return resolution;
}

void DesktopBackgroundController::MoveDesktopToLockedContainer() {
  if (locked_)
    return;
  locked_ = true;
  ReparentBackgroundWidgets(GetBackgroundContainerId(false),
                            GetBackgroundContainerId(true));
}

void DesktopBackgroundController::CleanupView(aura::RootWindow* root_window) {
  internal::ComponentWrapper* wrapper =
      root_window->GetProperty(internal::kComponentWrapper);
  if (NULL == wrapper)
    return;
  if (wrapper->GetComponent(false))
    wrapper->GetComponent(false)->CleanupWidget();
}

void DesktopBackgroundController::MoveDesktopToUnlockedContainer() {
  if (!locked_)
    return;
  locked_ = false;
  ReparentBackgroundWidgets(GetBackgroundContainerId(true),
                            GetBackgroundContainerId(false));
}

void DesktopBackgroundController::OnWindowDestroying(aura::Window* window) {
  window->SetProperty(internal::kWindowDesktopComponent,
      static_cast<internal::DesktopBackgroundWidgetController*>(NULL));
  window->SetProperty(internal::kComponentWrapper,
      static_cast<internal::ComponentWrapper*>(NULL));
}

void DesktopBackgroundController::SetDesktopBackgroundImageMode() {
  desktop_background_mode_ = BACKGROUND_IMAGE;
  InstallComponentForAllWindows();
}

void DesktopBackgroundController::OnWallpaperLoadCompleted(
    scoped_refptr<WallpaperOperation> wo) {
  current_wallpaper_.reset(wo->ReleaseWallpaperData());

  SetDesktopBackgroundImageMode();

  DCHECK(wo.get() == wallpaper_op_.get());
  wallpaper_op_ = NULL;
}

void DesktopBackgroundController::NotifyAnimationFinished() {
  Shell* shell = Shell::GetInstance();
  shell->user_wallpaper_delegate()->OnWallpaperAnimationFinished();
}

ui::Layer* DesktopBackgroundController::SetColorLayerForContainer(
    SkColor color,
    aura::RootWindow* root_window,
    int container_id) {
  ui::Layer* background_layer = new ui::Layer(ui::LAYER_SOLID_COLOR);
  background_layer->SetColor(color);

  Shell::GetContainer(root_window,container_id)->
      layer()->Add(background_layer);

  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&DesktopBackgroundController::NotifyAnimationFinished,
                 weak_ptr_factory_.GetWeakPtr()));

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
  if (NULL == root_window->GetProperty(internal::kComponentWrapper)) {
    // First time for this root window
    root_window->AddObserver(this);
  }
  root_window->SetProperty(internal::kComponentWrapper,
                           new internal::ComponentWrapper(component));
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
    if (root_window->GetProperty(internal::kComponentWrapper)) {
      internal::DesktopBackgroundWidgetController* component = root_window->
          GetProperty(internal::kWindowDesktopComponent);
      // Wallpaper animation may not finish at this point. Try to get component
      // from kComponentWrapper instead.
      if (!component) {
        component = root_window->GetProperty(internal::kComponentWrapper)->
            GetComponent(false);
      }
      DCHECK(component);
      component->Reparent(root_window,
                          src_container,
                          dst_container);
    }
  }
}

int DesktopBackgroundController::GetBackgroundContainerId(bool locked) {
  return locked ? internal::kShellWindowId_LockScreenBackgroundContainer :
                  internal::kShellWindowId_DesktopBackgroundContainer;
}

}  // namespace ash
