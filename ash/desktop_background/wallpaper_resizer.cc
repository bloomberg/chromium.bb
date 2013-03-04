// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/wallpaper_resizer.h"

#include "ash/desktop_background/wallpaper_resizer_observer.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/root_window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/skia_util.h"

using content::BrowserThread;

namespace ash {
namespace {

// Callback used to indicate that wallpaper has been resized.
typedef base::Callback<void(const SkBitmap&)> ResizedCallback;

// For our scaling ratios we need to round positive numbers.
int RoundPositive(double x) {
  return static_cast<int>(floor(x + 0.5));
}

// Resizes |wallpaper| to fit all screens and calls the callback.
void ResizeToFitScreens(const SkBitmap& wallpaper,
                        WallpaperLayout layout,
                        int width,
                        int height,
                        MessageLoop* origin_loop,
                        const ResizedCallback& callback) {
  SkBitmap resized_wallpaper = wallpaper;
  if (wallpaper.width() > width || wallpaper.height() > height) {
    gfx::Rect wallpaper_rect(0, 0, wallpaper.width(), wallpaper.height());
    gfx::Size cropped_size = gfx::Size(std::min(width, wallpaper.width()),
                                       std::min(height, wallpaper.height()));
    switch (layout) {
      case WALLPAPER_LAYOUT_CENTER:
        wallpaper_rect.ClampToCenteredSize(cropped_size);
        wallpaper.extractSubset(&resized_wallpaper,
                                gfx::RectToSkIRect(wallpaper_rect));
        break;
      case WALLPAPER_LAYOUT_TILE:
        wallpaper_rect.set_size(cropped_size);
        wallpaper.extractSubset(&resized_wallpaper,
                                gfx::RectToSkIRect(wallpaper_rect));
        break;
      case WALLPAPER_LAYOUT_STRETCH:
        resized_wallpaper = skia::ImageOperations::Resize(
            wallpaper, skia::ImageOperations::RESIZE_LANCZOS3,
            width, height);
        break;
      case WALLPAPER_LAYOUT_CENTER_CROPPED:
        if (wallpaper.width() > width && wallpaper.height() > height) {
          // The dimension with the smallest ratio must be cropped, the other
          // one is preserved. Both are set in gfx::Size cropped_size.
          double horizontal_ratio = static_cast<double>(width) /
              static_cast<double>(wallpaper.width());
          double vertical_ratio = static_cast<double>(height) /
              static_cast<double>(wallpaper.height());

          if (vertical_ratio > horizontal_ratio) {
            cropped_size = gfx::Size(
                RoundPositive(static_cast<double>(width) / vertical_ratio),
                wallpaper.height());
          } else {
            cropped_size = gfx::Size(wallpaper.width(),
                RoundPositive(static_cast<double>(height) / horizontal_ratio));
          }
          gfx::Rect wallpaper_cropped_rect = wallpaper_rect;
          wallpaper_cropped_rect.ClampToCenteredSize(cropped_size);
          SkBitmap sub_image;
          wallpaper.extractSubset(&sub_image,
                                  gfx::RectToSkIRect(wallpaper_rect));
          resized_wallpaper = skia::ImageOperations::Resize(
              sub_image, skia::ImageOperations::RESIZE_LANCZOS3,
              width, height);
        }
    }
  }
  resized_wallpaper.setImmutable();
  origin_loop->PostTask(FROM_HERE, base::Bind(callback, resized_wallpaper));
}

}  // namespace

WallpaperResizer::WallpaperResizer(const WallpaperInfo& info)
    : wallpaper_info_(info),
      wallpaper_image_(*(ui::ResourceBundle::GetSharedInstance().
          GetImageNamed(info.idr).ToImageSkia())),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

WallpaperResizer::WallpaperResizer(const WallpaperInfo& info,
                                   const gfx::ImageSkia& image)
    : wallpaper_info_(info),
      wallpaper_image_(image),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

WallpaperResizer::~WallpaperResizer() {
}

void WallpaperResizer::StartResize() {
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

  if (!BrowserThread::GetBlockingPool()->PostWorkerTaskWithShutdownBehavior(
      FROM_HERE,
      base::Bind(&ResizeToFitScreens,
                 *wallpaper_image_.bitmap(),
                 wallpaper_info_.layout,
                 width,
                 height,
                 MessageLoop::current(),
                 base::Bind(&WallpaperResizer::OnResizeFinished,
                            weak_ptr_factory_.GetWeakPtr())),
      base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN)) {
    LOG(WARNING) << "PostSequencedWorkerTask failed. " <<
                    "Wallpaper may not be resized.";
  }
}

void WallpaperResizer::AddObserver(WallpaperResizerObserver* observer) {
  observers_.AddObserver(observer);
}

void WallpaperResizer::RemoveObserver(WallpaperResizerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WallpaperResizer::OnResizeFinished(const SkBitmap& resized_wallpaper) {
  wallpaper_image_ = gfx::ImageSkia::CreateFrom1xBitmap(resized_wallpaper);
  FOR_EACH_OBSERVER(WallpaperResizerObserver, observers_,
                    OnWallpaperResized());
}

} // namespace ash
