// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/wallpaper_resizer.h"

#include "ash/desktop_background/wallpaper_resizer_observer.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/worker_pool.h"
#include "content/public/browser/browser_thread.h"
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

// Resizes |wallpaper| to |target_size| and calls the callback.
void Resize(const SkBitmap& wallpaper,
            WallpaperLayout layout,
            const gfx::Size& target_size,
            base::MessageLoop* origin_loop,
            const ResizedCallback& callback) {
  SkBitmap resized_wallpaper = wallpaper;
  int width = target_size.width();
  int height = target_size.height();
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
          wallpaper_rect.ClampToCenteredSize(cropped_size);
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

WallpaperResizer::WallpaperResizer(const WallpaperInfo& info,
                                   const gfx::Size& target_size)
    : wallpaper_info_(info),
      target_size_(target_size),
      wallpaper_image_(*(ui::ResourceBundle::GetSharedInstance().
          GetImageNamed(info.idr).ToImageSkia())),
      weak_ptr_factory_(this) {
}

WallpaperResizer::WallpaperResizer(const WallpaperInfo& info,
                                   const gfx::Size& target_size,
                                   const gfx::ImageSkia& image)
    : wallpaper_info_(info),
      target_size_(target_size),
      wallpaper_image_(image),
      weak_ptr_factory_(this) {
}

WallpaperResizer::~WallpaperResizer() {
}

void WallpaperResizer::StartResize() {
  if (!BrowserThread::GetBlockingPool()->PostWorkerTaskWithShutdownBehavior(
          FROM_HERE,
          base::Bind(&Resize,
                     *wallpaper_image_.bitmap(),
                     wallpaper_info_.layout,
                     target_size_,
                     base::MessageLoop::current(),
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
