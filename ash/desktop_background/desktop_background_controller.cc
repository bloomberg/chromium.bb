// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_controller.h"

#include "ash/desktop_background/desktop_background_controller_observer.h"
#include "ash/desktop_background/desktop_background_view.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/root_window_layout_manager.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/threading/worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/widget.h"

using ash::internal::DesktopBackgroundWidgetController;
using ash::internal::kAnimatingDesktopController;
using ash::internal::kDesktopController;
using content::BrowserThread;

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
      : is_default_wallpaper(true),
        wallpaper_index(index),
        wallpaper_layout(GetWallpaperViewInfo(index, resolution).layout),
        wallpaper_image(*(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            GetWallpaperViewInfo(index, resolution).id).ToImageSkia())) {
  }
  WallpaperData(WallpaperLayout layout, const gfx::ImageSkia& image)
      : is_default_wallpaper(false),
        wallpaper_index(-1),
        wallpaper_layout(layout),
        wallpaper_image(image) {
  }
  const bool is_default_wallpaper;
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

  int index() const {
    return index_;
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

void DesktopBackgroundController::AddObserver(
    DesktopBackgroundControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void DesktopBackgroundController::RemoveObserver(
    DesktopBackgroundControllerObserver* observer) {
  observers_.RemoveObserver(observer);
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
  if (BACKGROUND_IMAGE == desktop_background_mode_ &&
      current_wallpaper_.get()) {
    gfx::Size root_window_size = root_window->GetHostSize();
    // Loads a higher resolution wallpaper if new root window is larger than
    // small screen.
    if (kSmallWallpaperMaxWidth < root_window_size.width() ||
        kSmallWallpaperMaxHeight < root_window_size.height()) {
      if (current_wallpaper_->is_default_wallpaper) {
        ReloadDefaultWallpaper();
      } else {
        ash::Shell::GetInstance()->user_wallpaper_delegate()->
            UpdateWallpaper();
      }
    }
  }

  InstallDesktopController(root_window);
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

  // Prevents loading of the same wallpaper as the currently loading/loaded
  // one.
  if ((wallpaper_op_.get() && wallpaper_op_->index() == index) ||
      (current_wallpaper_.get() &&
       current_wallpaper_->wallpaper_index == index)) {
    return;
  }

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

void DesktopBackgroundController::ReloadDefaultWallpaper() {
  int index = current_wallpaper_->wallpaper_index;
  current_wallpaper_.reset(NULL);
  SetDefaultWallpaper(index);
}

void DesktopBackgroundController::SetCustomWallpaper(
    const gfx::ImageSkia& wallpaper,
    WallpaperLayout layout) {
  CancelPendingWallpaperOperation();
  if (current_wallpaper_.get() &&
      current_wallpaper_->wallpaper_image.BackedBySameObjectAs(wallpaper)) {
    return;
  }

  current_wallpaper_.reset(new WallpaperData(layout, wallpaper));
  FOR_EACH_OBSERVER(DesktopBackgroundControllerObserver, observers_,
                    OnWallpaperDataChanged());
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

  InstallDesktopControllerForAllWindows();
}

void DesktopBackgroundController::CreateEmptyWallpaper() {
  current_wallpaper_.reset(NULL);
  SetDesktopBackgroundImageMode();
}

WallpaperResolution DesktopBackgroundController::GetAppropriateResolution() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WallpaperResolution resolution = SMALL;
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    gfx::Size root_window_size = (*iter)->GetHostSize();
    if (root_window_size.width() > kSmallWallpaperMaxWidth ||
        root_window_size.height() > kSmallWallpaperMaxHeight) {
      resolution = LARGE;
    }
  }
  return resolution;
}

bool DesktopBackgroundController::MoveDesktopToLockedContainer() {
  if (locked_)
    return false;
  locked_ = true;
  return ReparentBackgroundWidgets(GetBackgroundContainerId(false),
                                   GetBackgroundContainerId(true));
}

bool DesktopBackgroundController::MoveDesktopToUnlockedContainer() {
  if (!locked_)
    return false;
  locked_ = false;
  return ReparentBackgroundWidgets(GetBackgroundContainerId(true),
                                   GetBackgroundContainerId(false));
}

void DesktopBackgroundController::OnWindowDestroying(aura::Window* window) {
  window->SetProperty(kDesktopController,
      static_cast<internal::DesktopBackgroundWidgetController*>(NULL));
  window->SetProperty(kAnimatingDesktopController,
      static_cast<internal::AnimatingDesktopController*>(NULL));
}

void DesktopBackgroundController::SetDesktopBackgroundImageMode() {
  desktop_background_mode_ = BACKGROUND_IMAGE;
  InstallDesktopControllerForAllWindows();
}

void DesktopBackgroundController::OnWallpaperLoadCompleted(
    scoped_refptr<WallpaperOperation> wo) {
  current_wallpaper_.reset(wo->ReleaseWallpaperData());
  FOR_EACH_OBSERVER(DesktopBackgroundControllerObserver, observers_,
                    OnWallpaperDataChanged());

  SetDesktopBackgroundImageMode();

  DCHECK(wo.get() == wallpaper_op_.get());
  wallpaper_op_ = NULL;
}

void DesktopBackgroundController::NotifyAnimationFinished() {
  Shell* shell = Shell::GetInstance();
  shell->GetPrimaryRootWindowController()->HandleDesktopBackgroundVisible();
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

void DesktopBackgroundController::InstallDesktopController(
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
  // Ensure we're only observing the root window once. Don't rely on a window
  // property check as those can be cleared by tests resetting the background.
  if (!root_window->HasObserver(this))
    root_window->AddObserver(this);

  internal::AnimatingDesktopController* animating_controller =
      root_window->GetProperty(kAnimatingDesktopController);
  if (animating_controller)
    animating_controller->StopAnimating();
  root_window->SetProperty(kAnimatingDesktopController,
                           new internal::AnimatingDesktopController(component));
}

void DesktopBackgroundController::InstallDesktopControllerForAllWindows() {
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    InstallDesktopController(*iter);
  }
}

bool DesktopBackgroundController::ReparentBackgroundWidgets(int src_container,
                                                            int dst_container) {
  bool moved = false;
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::iterator iter = root_windows.begin();
    iter != root_windows.end(); ++iter) {
    aura::RootWindow* root_window = *iter;
    // In the steady state (no animation playing) the background widget
    // controller exists in the kDesktopController property.
    DesktopBackgroundWidgetController* desktop_controller = root_window->
        GetProperty(kDesktopController);
    if (desktop_controller) {
      moved |= desktop_controller->Reparent(root_window,
                                            src_container,
                                            dst_container);
    }
    // During desktop show animations the controller lives in
    // kAnimatingDesktopController.
    // NOTE: If a wallpaper load happens during a desktop show animation there
    // can temporarily be two desktop background widgets.  We must reparent
    // both of them - one above and one here.
    DesktopBackgroundWidgetController* animating_controller =
        root_window->GetProperty(kAnimatingDesktopController) ?
        root_window->GetProperty(kAnimatingDesktopController)->
            GetController(false) :
        NULL;
    if (animating_controller) {
      moved |= animating_controller->Reparent(root_window,
                                              src_container,
                                              dst_container);
    }
  }
  return moved;
}

int DesktopBackgroundController::GetBackgroundContainerId(bool locked) {
  return locked ? internal::kShellWindowId_LockScreenBackgroundContainer :
                  internal::kShellWindowId_DesktopBackgroundContainer;
}

}  // namespace ash
