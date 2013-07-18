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
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/property_util.h"
#include "ash/wm/root_window_layout_manager.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/threading/worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "grit/ash_resources.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/widget.h"

using ash::internal::DesktopBackgroundWidgetController;
using content::BrowserThread;

namespace ash {
namespace {

const SkColor kTransparentColor = SkColorSetARGB(0x00, 0x00, 0x00, 0x00);

internal::RootWindowLayoutManager* GetRootWindowLayoutManager(
    aura::RootWindow* root_window) {
  return static_cast<internal::RootWindowLayoutManager*>(
      root_window->layout_manager());
}

// Returns the maximum width and height of all root windows.
gfx::Size GetRootWindowsSize() {
  int width = 0;
  int height = 0;
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    gfx::Size root_window_size = (*iter)->GetHostSize();
    if (root_window_size.width() > width)
      width = root_window_size.width();
    if (root_window_size.height() > height)
      height = root_window_size.height();
  }
  return gfx::Size(width, height);
}

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

  static void LoadOnWorkerPoolThread(scoped_refptr<WallpaperLoader> loader) {
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
    loader->LoadWallpaper();
  }

  const base::FilePath& file_path() const { return file_path_; }
  int resource_id() const { return resource_id_; }

  void Cancel() {
    cancel_flag_.Set();
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
    if (!file_util::ReadFileToString(path, &data)) {
      LOG(ERROR) << "Unable to read data from " << path.value();
      return scoped_ptr<SkBitmap>();
    }

    scoped_ptr<SkBitmap> bitmap(gfx::JPEGCodec::Decode(
        reinterpret_cast<const unsigned char*>(data.data()), data.size()));
    if (!bitmap)
      LOG(ERROR) << "Unable to decode JPEG data from " << path.value();
    return bitmap.Pass();
  }

  void LoadWallpaper() {
    if (cancel_flag_.IsSet())
      return;

    if (!file_path_.empty())
      file_bitmap_ = LoadSkBitmapFromJPEGFile(file_path_);

    if (cancel_flag_.IsSet())
      return;

    if (file_bitmap_) {
      gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(*file_bitmap_);
      wallpaper_resizer_.reset(new WallpaperResizer(
          image, GetRootWindowsSize(), file_layout_));
    } else {
      wallpaper_resizer_.reset(new WallpaperResizer(
          resource_id_, GetRootWindowsSize(), resource_layout_));
    }
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
      background_color_(kTransparentColor),
      current_default_wallpaper_resource_id_(-1),
      weak_ptr_factory_(this) {
}

DesktopBackgroundController::~DesktopBackgroundController() {
  CancelPendingWallpaperOperation();
}

gfx::ImageSkia DesktopBackgroundController::GetWallpaper() const {
  if (current_wallpaper_)
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
  if (current_wallpaper_)
    return current_wallpaper_->layout();
  return WALLPAPER_LAYOUT_CENTER_CROPPED;
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
      current_default_wallpaper_path_ = base::FilePath();
      current_default_wallpaper_resource_id_ = -1;
      ash::Shell::GetInstance()->user_wallpaper_delegate()->
          UpdateWallpaper();
    }
  }

  InstallDesktopController(root_window);
}

bool DesktopBackgroundController::SetDefaultWallpaper(bool is_guest) {
  const bool use_large =
      GetAppropriateResolution() == WALLPAPER_RESOLUTION_LARGE;

  base::FilePath file_path;
  WallpaperLayout file_layout = use_large ? WALLPAPER_LAYOUT_CENTER_CROPPED :
      WALLPAPER_LAYOUT_CENTER;
  int resource_id = use_large ? IDR_AURA_WALLPAPER_DEFAULT_LARGE :
      IDR_AURA_WALLPAPER_DEFAULT_SMALL;
  WallpaperLayout resource_layout = WALLPAPER_LAYOUT_TILE;

  const char* switch_name = is_guest ?
      (use_large ? switches::kAshDefaultGuestWallpaperLarge :
       switches::kAshDefaultGuestWallpaperSmall) :
      (use_large ? switches::kAshDefaultWallpaperLarge :
       switches::kAshDefaultWallpaperSmall);
  CommandLine* command_line = command_line_for_testing_ ?
      command_line_for_testing_ : CommandLine::ForCurrentProcess();
  file_path = command_line->GetSwitchValuePath(switch_name);

  if (DefaultWallpaperIsAlreadyLoadingOrLoaded(file_path, resource_id))
    return false;

  CancelPendingWallpaperOperation();
  wallpaper_loader_ = new WallpaperLoader(
      file_path, file_layout, resource_id, resource_layout);
  base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WallpaperLoader::LoadOnWorkerPoolThread, wallpaper_loader_),
      base::Bind(&DesktopBackgroundController::OnDefaultWallpaperLoadCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 wallpaper_loader_),
      true /* task_is_slow */);
  return true;
}

void DesktopBackgroundController::SetCustomWallpaper(
    const gfx::ImageSkia& image,
    WallpaperLayout layout) {
  CancelPendingWallpaperOperation();
  if (CustomWallpaperIsAlreadyLoaded(image))
    return;

  current_wallpaper_.reset(new WallpaperResizer(
      image, GetRootWindowsSize(), layout));
  current_wallpaper_->StartResize();

  current_default_wallpaper_path_ = base::FilePath();
  current_default_wallpaper_resource_id_ = -1;

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
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    // Compare to host size as constants are defined in terms of
    // physical pixel size.
    // TODO(oshima): This may not be ideal for fractional scaling
    // scenario. Revisit and fix if necessary.
    gfx::Size host_window_size = (*iter)->GetHostSize();
    if (host_window_size.width() > kSmallWallpaperMaxWidth ||
        host_window_size.height() > kSmallWallpaperMaxHeight)
      return WALLPAPER_RESOLUTION_LARGE;
  }
  return WALLPAPER_RESOLUTION_SMALL;
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

bool DesktopBackgroundController::DefaultWallpaperIsAlreadyLoadingOrLoaded(
    const base::FilePath& image_file, int image_resource_id) const {
  return (wallpaper_loader_.get() &&
          wallpaper_loader_->file_path() == image_file &&
          wallpaper_loader_->resource_id() == image_resource_id) ||
         (current_wallpaper_.get() &&
          current_default_wallpaper_path_ == image_file &&
          current_default_wallpaper_resource_id_ == image_resource_id);
}

bool DesktopBackgroundController::CustomWallpaperIsAlreadyLoaded(
    const gfx::ImageSkia& image) const {
  return current_wallpaper_.get() &&
      current_wallpaper_->wallpaper_image().BackedBySameObjectAs(image);
}

void DesktopBackgroundController::SetDesktopBackgroundImageMode() {
  desktop_background_mode_ = BACKGROUND_IMAGE;
  InstallDesktopControllerForAllWindows();
}

void DesktopBackgroundController::OnDefaultWallpaperLoadCompleted(
    scoped_refptr<WallpaperLoader> loader) {
  current_wallpaper_.reset(loader->ReleaseWallpaperResizer());
  current_wallpaper_->StartResize();
  current_default_wallpaper_path_ = loader->file_path();
  current_default_wallpaper_resource_id_ = loader->resource_id();
  FOR_EACH_OBSERVER(DesktopBackgroundControllerObserver, observers_,
                    OnWallpaperDataChanged());

  SetDesktopBackgroundImageMode();

  DCHECK(loader.get() == wallpaper_loader_.get());
  wallpaper_loader_ = NULL;
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
  GetRootWindowController(root_window)->SetAnimatingWallpaperController(
      new internal::AnimatingDesktopController(component));

  component->StartAnimating(GetRootWindowController(root_window));
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
  Shell::RootWindowControllerList controllers =
      Shell::GetAllRootWindowControllers();
  for (Shell::RootWindowControllerList::iterator iter = controllers.begin();
    iter != controllers.end(); ++iter) {
    internal::RootWindowController* root_window_controller = *iter;
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
  return locked ? internal::kShellWindowId_LockScreenBackgroundContainer :
                  internal::kShellWindowId_DesktopBackgroundContainer;
}

}  // namespace ash
