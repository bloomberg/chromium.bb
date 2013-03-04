// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_WALLPAPER_RESIZER_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_WALLPAPER_RESIZER_H_

#include "ash/ash_export.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

class WallpaperResizerObserver;

// Stores the current wallpaper data and resize it to fit all screens if needed.
class ASH_EXPORT WallpaperResizer {
 public:
  explicit WallpaperResizer(const WallpaperInfo& info);

  WallpaperResizer(const WallpaperInfo& info, const gfx::ImageSkia& image);

  virtual ~WallpaperResizer();

  const WallpaperInfo& wallpaper_info() const { return wallpaper_info_; }

  const gfx::ImageSkia& wallpaper_image() const { return wallpaper_image_; }

  // Starts resize task on UI thread. It posts task to worker pool.
  void StartResize();

  // Add/Remove observers.
  void AddObserver(WallpaperResizerObserver* observer);
  void RemoveObserver(WallpaperResizerObserver* observer);

 private:
  // Replaces the orginal uncompressed wallpaper to |resized_wallpaper|.
  void OnResizeFinished(const SkBitmap& resized_wallpaper);

  ObserverList<WallpaperResizerObserver> observers_;

  const WallpaperInfo wallpaper_info_;

  gfx::ImageSkia wallpaper_image_;

  base::WeakPtrFactory<WallpaperResizer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperResizer);
};

}  // namespace ash


#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_WALLPAPER_RESIZER_H_
