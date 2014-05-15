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
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/size.h"

namespace ash {

class WallpaperResizerObserver;

// Stores the current wallpaper data and resize it to |target_size| if needed.
class ASH_EXPORT WallpaperResizer {
 public:
  // Returns a unique identifier corresponding to |image|, suitable for
  // comparison against the value returned by original_image_id(). If the image
  // is modified, its ID will change.
  static uint32_t GetImageId(const gfx::ImageSkia& image);

  WallpaperResizer(const gfx::ImageSkia& image,
                   const gfx::Size& target_size,
                   WallpaperLayout layout);

  ~WallpaperResizer();

  const gfx::ImageSkia& image() const { return image_; }
  uint32_t original_image_id() const { return original_image_id_; }
  WallpaperLayout layout() const { return layout_; }

  // Called on the UI thread to run Resize() on the worker pool and post an
  // OnResizeFinished() task back to the UI thread on completion.
  void StartResize();

  // Add/Remove observers.
  void AddObserver(WallpaperResizerObserver* observer);
  void RemoveObserver(WallpaperResizerObserver* observer);

 private:
  // Copies |resized_bitmap| to |image_| and notifies observers after Resize()
  // has finished running.
  void OnResizeFinished(SkBitmap* resized_bitmap);

  ObserverList<WallpaperResizerObserver> observers_;

  // Image that should currently be used for wallpaper. It initially
  // contains the original image and is updated to contain the resized
  // image by OnResizeFinished().
  gfx::ImageSkia image_;

  // Unique identifier corresponding to the original (i.e. pre-resize) |image_|.
  uint32_t original_image_id_;

  gfx::Size target_size_;

  WallpaperLayout layout_;

  base::WeakPtrFactory<WallpaperResizer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperResizer);
};

}  // namespace ash


#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_WALLPAPER_RESIZER_H_
