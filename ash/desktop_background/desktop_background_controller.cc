// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_controller.h"

#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_controller_observer.h"
#include "ash/desktop_background/desktop_background_view.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/desktop_background/wallpaper_resizer.h"
#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/root_window_layout_manager.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/threading/worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "grit/ash_resources.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/widget.h"

using content::BrowserThread;

namespace ash {
namespace {

// How long to wait reloading the wallpaper after the max display has
// changed?
const int kWallpaperReloadDelayMs = 2000;

}  // namespace

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
  // If set, |file_path| must be a trusted (i.e. read-only,
  // non-user-controlled) file containing a JPEG image.
  WallpaperLoader(const base::FilePath& file_path,
                  WallpaperLayout file_layout,
                  int resource_id,
                  WallpaperLayout resource_layout)
      : file_path_(file_path),
        file_layout_(file_layout),
        resource_id_(resource_id),
        resource_layout_(resource_layout) {
  }

  void LoadOnWorkerPoolThread() {
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (cancel_flag_.IsSet())
      return;

    if (!file_path_.empty()) {
      VLOG(1) << "Loading " << file_path_.value();
      file_bitmap_ = LoadSkBitmapFromJPEGFile(file_path_);
    }

    if (cancel_flag_.IsSet())
      return;

    if (file_bitmap_) {
      gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(*file_bitmap_);
      wallpaper_resizer_.reset(new WallpaperResizer(
          image, GetMaxDisplaySizeInNative(), file_layout_));
    } else {
      wallpaper_resizer_.reset(new WallpaperResizer(
          resource_id_, GetMaxDisplaySizeInNative(), resource_layout_));
    }
  }

  const base::FilePath& file_path() const { return file_path_; }
  int resource_id() const { return resource_id_; }

  void Cancel() {
    cancel_flag_.Set();
  }

  bool IsCanceled() {
    return cancel_flag_.IsSet();
  }

  WallpaperResizer* ReleaseWallpaperResizer() {
    return wallpaper_resizer_.release();
  }

 private:
  friend class base::RefCountedThreadSafe<
      DesktopBackgroundController::WallpaperLoader>;

  // Loads a JPEG image from |path|, a trusted file -- note that the image
  // is not loaded in a sandboxed process. Returns an empty pointer on
  // error.
  static scoped_ptr<SkBitmap> LoadSkBitmapFromJPEGFile(
      const base::FilePath& path) {
    std::string data;
    if (!base::ReadFileToString(path, &data)) {
      LOG(ERROR) << "Unable to read data from " << path.value();
      return scoped_ptr<SkBitmap>();
    }

    scoped_ptr<SkBitmap> bitmap(gfx::JPEGCodec::Decode(
        reinterpret_cast<const unsigned char*>(data.data()), data.size()));
    if (!bitmap)
      LOG(ERROR) << "Unable to decode JPEG data from " << path.value();
    return bitmap.Pass();
  }

  ~WallpaperLoader() {}

  base::CancellationFlag cancel_flag_;

  // Bitmap loaded from |file_path_|.
  scoped_ptr<SkBitmap> file_bitmap_;

  scoped_ptr<WallpaperResizer> wallpaper_resizer_;

  // Path to a trusted JPEG file.
  base::FilePath file_path_;

  // Layout to be used when displaying the image from |file_path_|.
  WallpaperLayout file_layout_;

  // ID of an image resource to use if |file_path_| is empty or unloadable.
  int resource_id_;

  // Layout to be used when displaying |resource_id_|.
  WallpaperLayout resource_layout_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperLoader);
};

DesktopBackgroundController::DesktopBackgroundController()
    : command_line_for_testing_(NULL),
      locked_(false),
      desktop_background_mode_(BACKGROUND_NONE),
      current_default_wallpaper_resource_id_(-1),
      weak_ptr_factory_(this),
      wallpaper_reload_delay_(kWallpaperReloadDelayMs) {
  Shell::GetInstance()->display_controller()->AddObserver(this);
}

DesktopBackgroundController::~DesktopBackgroundController() {
  CancelDefaultWallpaperLoader();
  Shell::GetInstance()->display_controller()->RemoveObserver(this);
}

gfx::ImageSkia DesktopBackgroundController::GetWallpaper() const {
  if (current_wallpaper_)
    return current_wallpaper_->image();
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
  if (current_wallpaper_)
    return current_wallpaper_->layout();
  return WALLPAPER_LAYOUT_CENTER_CROPPED;
}

void DesktopBackgroundController::OnRootWindowAdded(aura::Window* root_window) {
  // The background hasn't been set yet.
  if (desktop_background_mode_ == BACKGROUND_NONE)
    return;

  // Handle resolution change for "built-in" images.
  gfx::Size max_display_size = GetMaxDisplaySizeInNative();
  if (current_max_display_size_ != max_display_size) {
    current_max_display_size_ = max_display_size;
    if (desktop_background_mode_ == BACKGROUND_IMAGE &&
        current_wallpaper_.get())
      UpdateWallpaper();
  }

  InstallDesktopController(root_window);
}

bool DesktopBackgroundController::SetDefaultWallpaper(bool is_guest) {
  VLOG(1) << "SetDefaultWallpaper: is_guest=" << is_guest;
  const bool use_large =
      GetAppropriateResolution() == WALLPAPER_RESOLUTION_LARGE;

  base::FilePath file_path;
  WallpaperLayout file_layout = use_large ? WALLPAPER_LAYOUT_CENTER_CROPPED :
      WALLPAPER_LAYOUT_CENTER;
  int resource_id = use_large ? IDR_AURA_WALLPAPER_DEFAULT_LARGE :
      IDR_AURA_WALLPAPER_DEFAULT_SMALL;
  WallpaperLayout resource_layout = WALLPAPER_LAYOUT_TILE;

  CommandLine* command_line = command_line_for_testing_ ?
      command_line_for_testing_ : CommandLine::ForCurrentProcess();
  const char* switch_name = NULL;
  if (is_guest) {
    switch_name = use_large ? switches::kAshGuestWallpaperLarge :
        switches::kAshGuestWallpaperSmall;
  } else {
    switch_name = use_large ? switches::kAshDefaultWallpaperLarge :
        switches::kAshDefaultWallpaperSmall;
  }
  file_path = command_line->GetSwitchValuePath(switch_name);

  if (DefaultWallpaperIsAlreadyLoadingOrLoaded(file_path, resource_id)) {
    VLOG(1) << "Default wallpaper is already loading or loaded";
    return false;
  }

  CancelDefaultWallpaperLoader();
  default_wallpaper_loader_ = new WallpaperLoader(
      file_path, file_layout, resource_id, resource_layout);
  base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WallpaperLoader::LoadOnWorkerPoolThread,
                 default_wallpaper_loader_),
      base::Bind(&DesktopBackgroundController::OnDefaultWallpaperLoadCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 default_wallpaper_loader_),
      true /* task_is_slow */);
  return true;
}

void DesktopBackgroundController::SetCustomWallpaper(
    const gfx::ImageSkia& image,
    WallpaperLayout layout) {
  VLOG(1) << "SetCustomWallpaper: image_id="
          << WallpaperResizer::GetImageId(image) << " layout=" << layout;
  CancelDefaultWallpaperLoader();

  if (CustomWallpaperIsAlreadyLoaded(image)) {
    VLOG(1) << "Custom wallpaper is already loaded";
    return;
  }

  current_wallpaper_.reset(new WallpaperResizer(
      image, GetMaxDisplaySizeInNative(), layout));
  current_wallpaper_->StartResize();

  current_default_wallpaper_path_ = base::FilePath();
  current_default_wallpaper_resource_id_ = -1;

  FOR_EACH_OBSERVER(DesktopBackgroundControllerObserver, observers_,
                    OnWallpaperDataChanged());
  SetDesktopBackgroundImageMode();
}

void DesktopBackgroundController::CancelDefaultWallpaperLoader() {
  // Set canceled flag of previous request to skip unneeded loading.
  if (default_wallpaper_loader_.get())
    default_wallpaper_loader_->Cancel();

  // Cancel reply callback for previous request.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void DesktopBackgroundController::CreateEmptyWallpaper() {
  current_wallpaper_.reset(NULL);
  SetDesktopBackgroundImageMode();
}

WallpaperResolution DesktopBackgroundController::GetAppropriateResolution() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  gfx::Size size = GetMaxDisplaySizeInNative();
  return (size.width() > kSmallWallpaperMaxWidth ||
          size.height() > kSmallWallpaperMaxHeight) ?
      WALLPAPER_RESOLUTION_LARGE : WALLPAPER_RESOLUTION_SMALL;
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

void DesktopBackgroundController::OnDisplayConfigurationChanged() {
  gfx::Size max_display_size = GetMaxDisplaySizeInNative();
  if (current_max_display_size_ != max_display_size) {
    current_max_display_size_ = max_display_size;
    if (desktop_background_mode_ == BACKGROUND_IMAGE &&
        current_wallpaper_.get()) {
      timer_.Stop();
      timer_.Start(FROM_HERE,
                   base::TimeDelta::FromMilliseconds(wallpaper_reload_delay_),
                   this,
                   &DesktopBackgroundController::UpdateWallpaper);
    }
  }
}

bool DesktopBackgroundController::DefaultWallpaperIsAlreadyLoadingOrLoaded(
    const base::FilePath& image_file,
    int image_resource_id) const {
  return (default_wallpaper_loader_.get() &&
          !default_wallpaper_loader_->IsCanceled() &&
          default_wallpaper_loader_->file_path() == image_file &&
          default_wallpaper_loader_->resource_id() == image_resource_id) ||
         (current_wallpaper_.get() &&
          current_default_wallpaper_path_ == image_file &&
          current_default_wallpaper_resource_id_ == image_resource_id);
}

bool DesktopBackgroundController::CustomWallpaperIsAlreadyLoaded(
    const gfx::ImageSkia& image) const {
  return current_wallpaper_.get() &&
      (WallpaperResizer::GetImageId(image) ==
       current_wallpaper_->original_image_id());
}

void DesktopBackgroundController::SetDesktopBackgroundImageMode() {
  desktop_background_mode_ = BACKGROUND_IMAGE;
  InstallDesktopControllerForAllWindows();
}

void DesktopBackgroundController::OnDefaultWallpaperLoadCompleted(
    scoped_refptr<WallpaperLoader> loader) {
  VLOG(1) << "OnDefaultWallpaperLoadCompleted";
  current_wallpaper_.reset(loader->ReleaseWallpaperResizer());
  current_wallpaper_->StartResize();
  current_default_wallpaper_path_ = loader->file_path();
  current_default_wallpaper_resource_id_ = loader->resource_id();
  FOR_EACH_OBSERVER(DesktopBackgroundControllerObserver, observers_,
                    OnWallpaperDataChanged());

  SetDesktopBackgroundImageMode();

  DCHECK(loader.get() == default_wallpaper_loader_.get());
  default_wallpaper_loader_ = NULL;
}

void DesktopBackgroundController::InstallDesktopController(
    aura::Window* root_window) {
  DesktopBackgroundWidgetController* component = NULL;
  int container_id = GetBackgroundContainerId(locked_);

  switch (desktop_background_mode_) {
    case BACKGROUND_IMAGE: {
      views::Widget* widget =
          CreateDesktopBackground(root_window, container_id);
      component = new DesktopBackgroundWidgetController(widget);
      break;
    }
    case BACKGROUND_NONE:
      NOTREACHED();
      return;
  }
  GetRootWindowController(root_window)->SetAnimatingWallpaperController(
      new AnimatingDesktopController(component));

  component->StartAnimating(GetRootWindowController(root_window));
}

void DesktopBackgroundController::InstallDesktopControllerForAllWindows() {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    InstallDesktopController(*iter);
  }
  current_max_display_size_ = GetMaxDisplaySizeInNative();
}

bool DesktopBackgroundController::ReparentBackgroundWidgets(int src_container,
                                                            int dst_container) {
  bool moved = false;
  Shell::RootWindowControllerList controllers =
      Shell::GetAllRootWindowControllers();
  for (Shell::RootWindowControllerList::iterator iter = controllers.begin();
    iter != controllers.end(); ++iter) {
    RootWindowController* root_window_controller = *iter;
    // In the steady state (no animation playing) the background widget
    // controller exists in the RootWindowController.
    DesktopBackgroundWidgetController* desktop_controller =
        root_window_controller->wallpaper_controller();
    if (desktop_controller) {
      moved |= desktop_controller->Reparent(
          root_window_controller->root_window(),
          src_container,
          dst_container);
    }
    // During desktop show animations the controller lives in
    // AnimatingDesktopController owned by RootWindowController.
    // NOTE: If a wallpaper load happens during a desktop show animation there
    // can temporarily be two desktop background widgets.  We must reparent
    // both of them - one above and one here.
    DesktopBackgroundWidgetController* animating_controller =
        root_window_controller->animating_wallpaper_controller() ?
        root_window_controller->animating_wallpaper_controller()->
            GetController(false) :
        NULL;
    if (animating_controller) {
      moved |= animating_controller->Reparent(
          root_window_controller->root_window(),
          src_container,
          dst_container);
    }
  }
  return moved;
}

int DesktopBackgroundController::GetBackgroundContainerId(bool locked) {
  return locked ? kShellWindowId_LockScreenBackgroundContainer
                : kShellWindowId_DesktopBackgroundContainer;
}

void DesktopBackgroundController::UpdateWallpaper() {
  current_wallpaper_.reset(NULL);
  current_default_wallpaper_path_ = base::FilePath();
  current_default_wallpaper_resource_id_ = -1;
  ash::Shell::GetInstance()->user_wallpaper_delegate()->
      UpdateWallpaper(true /* clear cache */);
}

// static
gfx::Size DesktopBackgroundController::GetMaxDisplaySizeInNative() {
  int width = 0;
  int height = 0;
  std::vector<gfx::Display> displays = Shell::GetScreen()->GetAllDisplays();
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();

  for (std::vector<gfx::Display>::iterator iter = displays.begin();
       iter != displays.end(); ++iter) {
    // Don't use size_in_pixel because we want to use the native pixel size.
    gfx::Size size_in_pixel =
        display_manager->GetDisplayInfo(iter->id()).bounds_in_native().size();
    if (iter->rotation() == gfx::Display::ROTATE_90 ||
        iter->rotation() == gfx::Display::ROTATE_270) {
      size_in_pixel = gfx::Size(size_in_pixel.height(), size_in_pixel.width());
    }
    width = std::max(size_in_pixel.width(), width);
    height = std::max(size_in_pixel.height(), height);
  }
  return gfx::Size(width, height);
}

}  // namespace ash
