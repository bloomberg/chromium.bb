// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_controller.h"

#include "ash/desktop_background/desktop_background_controller_observer.h"
#include "ash/desktop_background/desktop_background_view.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/desktop_background/wallpaper_resizer.h"
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
#include "grit/ash_wallpaper_resources.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
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

#if defined(GOOGLE_CHROME_BUILD)
const WallpaperInfo kDefaultLargeWallpaper =
    { IDR_AURA_WALLPAPERS_2_LANDSCAPE8_LARGE, WALLPAPER_LAYOUT_CENTER_CROPPED };
const WallpaperInfo kDefaultSmallWallpaper =
    { IDR_AURA_WALLPAPERS_2_LANDSCAPE8_SMALL, WALLPAPER_LAYOUT_CENTER };
const WallpaperInfo kGuestLargeWallpaper =
    { IDR_AURA_WALLPAPERS_2_LANDSCAPE7_LARGE, WALLPAPER_LAYOUT_CENTER_CROPPED };
const WallpaperInfo kGuestSmallWallpaper =
    { IDR_AURA_WALLPAPERS_2_LANDSCAPE7_SMALL, WALLPAPER_LAYOUT_CENTER };
#else
const WallpaperInfo kDefaultLargeWallpaper =
    { IDR_AURA_WALLPAPERS_5_GRADIENT5_LARGE, WALLPAPER_LAYOUT_TILE };
const WallpaperInfo kDefaultSmallWallpaper =
    { IDR_AURA_WALLPAPERS_5_GRADIENT5_SMALL, WALLPAPER_LAYOUT_TILE };
const WallpaperInfo kGuestLargeWallpaper = kDefaultLargeWallpaper;
const WallpaperInfo kGuestSmallWallpaper = kDefaultSmallWallpaper;
#endif

const int kSmallWallpaperMaxWidth = 1366;
const int kSmallWallpaperMaxHeight = 800;
const int kLargeWallpaperMaxWidth = 2560;
const int kLargeWallpaperMaxHeight = 1700;
const int kWallpaperThumbnailWidth = 108;
const int kWallpaperThumbnailHeight = 68;

// DesktopBackgroundController::WallpaperLoader wraps background wallpaper
// loading.
class DesktopBackgroundController::WallpaperLoader
    : public base::RefCountedThreadSafe<
          DesktopBackgroundController::WallpaperLoader> {
 public:
  explicit WallpaperLoader(const WallpaperInfo& info)
      : info_(info) {
  }

  static void LoadOnWorkerPoolThread(scoped_refptr<WallpaperLoader> wl) {
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
    wl->LoadingWallpaper();
  }

  void Cancel() {
    cancel_flag_.Set();
  }

  int idr() const {
    return info_.idr;
  }

  WallpaperResizer* ReleaseWallpaperResizer() {
    return wallpaper_resizer_.release();
  }

 private:
  friend class base::RefCountedThreadSafe<
      DesktopBackgroundController::WallpaperLoader>;

  void LoadingWallpaper() {
    if (cancel_flag_.IsSet())
      return;
    wallpaper_resizer_.reset(new WallpaperResizer(info_));
  }

  ~WallpaperLoader() {}

  base::CancellationFlag cancel_flag_;

  scoped_ptr<WallpaperResizer> wallpaper_resizer_;

  const WallpaperInfo info_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperLoader);
};

DesktopBackgroundController::DesktopBackgroundController()
    : locked_(false),
      desktop_background_mode_(BACKGROUND_NONE),
      background_color_(kTransparentColor),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

DesktopBackgroundController::~DesktopBackgroundController() {
  CancelPendingWallpaperOperation();
}

gfx::ImageSkia DesktopBackgroundController::GetWallpaper() const {
  if (current_wallpaper_.get())
    return current_wallpaper_->wallpaper_image();
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
    return current_wallpaper_->wallpaper_info().layout;
  return WALLPAPER_LAYOUT_CENTER_CROPPED;
}

gfx::ImageSkia DesktopBackgroundController::GetCurrentWallpaperImage() {
  if (desktop_background_mode_ != BACKGROUND_IMAGE)
    return gfx::ImageSkia();
  return GetWallpaper();
}

int DesktopBackgroundController::GetWallpaperIDR() const {
  if (wallpaper_loader_.get())
    return wallpaper_loader_->idr();
  else if (current_wallpaper_.get())
    return current_wallpaper_->wallpaper_info().idr;
  else
    return -1;
}

void DesktopBackgroundController::OnRootWindowAdded(
    aura::RootWindow* root_window) {
  // The background hasn't been set yet.
  if (desktop_background_mode_ == BACKGROUND_NONE)
    return;

  // Handle resolution change for "built-in" images.
  if (BACKGROUND_IMAGE == desktop_background_mode_ &&
      current_wallpaper_.get()) {
    gfx::Size root_window_size = root_window->GetHostSize();
    int width = current_wallpaper_->wallpaper_image().width();
    int height = current_wallpaper_->wallpaper_image().height();
    // Reloads wallpaper if current wallpaper is smaller than the new added root
    // window.
    if (width < root_window_size.width() ||
        height < root_window_size.height()) {
      current_wallpaper_.reset(NULL);
      ash::Shell::GetInstance()->user_wallpaper_delegate()->
          UpdateWallpaper();
    }
  }

  InstallDesktopController(root_window);
}

void DesktopBackgroundController::SetDefaultWallpaper(
    const WallpaperInfo& info) {
  DCHECK_NE(GetWallpaperIDR(), info.idr);

  CancelPendingWallpaperOperation();
  wallpaper_loader_ = new WallpaperLoader(info);
  base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WallpaperLoader::LoadOnWorkerPoolThread, wallpaper_loader_),
      base::Bind(&DesktopBackgroundController::OnWallpaperLoadCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 wallpaper_loader_),
      true /* task_is_slow */);
}

void DesktopBackgroundController::SetCustomWallpaper(
    const gfx::ImageSkia& wallpaper,
    WallpaperLayout layout) {
  CancelPendingWallpaperOperation();
  if (current_wallpaper_.get() &&
      current_wallpaper_->wallpaper_image().BackedBySameObjectAs(wallpaper)) {
    return;
  }

  WallpaperInfo info = { -1, layout };
  current_wallpaper_.reset(new WallpaperResizer(info, wallpaper));
  current_wallpaper_->StartResize();
  FOR_EACH_OBSERVER(DesktopBackgroundControllerObserver, observers_,
                    OnWallpaperDataChanged());
  SetDesktopBackgroundImageMode();
}

void DesktopBackgroundController::CancelPendingWallpaperOperation() {
  // Set canceled flag of previous request to skip unneeded loading.
  if (wallpaper_loader_.get())
    wallpaper_loader_->Cancel();

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
  WallpaperResolution resolution = WALLPAPER_RESOLUTION_SMALL;
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    gfx::Size root_window_size = (*iter)->GetHostSize();
    if (root_window_size.width() > kSmallWallpaperMaxWidth ||
        root_window_size.height() > kSmallWallpaperMaxHeight) {
      resolution = WALLPAPER_RESOLUTION_LARGE;
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
    scoped_refptr<WallpaperLoader> wl) {
  current_wallpaper_.reset(wl->ReleaseWallpaperResizer());
  FOR_EACH_OBSERVER(DesktopBackgroundControllerObserver, observers_,
                    OnWallpaperDataChanged());

  SetDesktopBackgroundImageMode();

  DCHECK(wl.get() == wallpaper_loader_.get());
  wallpaper_loader_ = NULL;
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
    case BACKGROUND_NONE:
      NOTREACHED();
      return;
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
