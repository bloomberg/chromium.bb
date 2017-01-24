// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_INTERFACES_WALLPAPER_STRUCT_TRAITS_H_
#define ASH_PUBLIC_INTERFACES_WALLPAPER_STRUCT_TRAITS_H_

#include "ash/public/interfaces/wallpaper.mojom.h"
#include "components/wallpaper/wallpaper_layout.h"

namespace mojo {

template <>
struct EnumTraits<ash::mojom::WallpaperLayout, wallpaper::WallpaperLayout> {
  static ash::mojom::WallpaperLayout ToMojom(wallpaper::WallpaperLayout input) {
    switch (input) {
      case wallpaper::WALLPAPER_LAYOUT_CENTER:
        return ash::mojom::WallpaperLayout::CENTER;
      case wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED:
        return ash::mojom::WallpaperLayout::CENTER_CROPPED;
      case wallpaper::WALLPAPER_LAYOUT_STRETCH:
        return ash::mojom::WallpaperLayout::STRETCH;
      case wallpaper::WALLPAPER_LAYOUT_TILE:
        return ash::mojom::WallpaperLayout::TILE;
      case wallpaper::NUM_WALLPAPER_LAYOUT:
        break;
    }
    NOTREACHED();
    return ash::mojom::WallpaperLayout::CENTER;
  }

  static bool FromMojom(ash::mojom::WallpaperLayout input,
                        wallpaper::WallpaperLayout* out) {
    switch (input) {
      case ash::mojom::WallpaperLayout::CENTER:
        *out = wallpaper::WALLPAPER_LAYOUT_CENTER;
        return true;
      case ash::mojom::WallpaperLayout::CENTER_CROPPED:
        *out = wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
        return true;
      case ash::mojom::WallpaperLayout::STRETCH:
        *out = wallpaper::WALLPAPER_LAYOUT_STRETCH;
        return true;
      case ash::mojom::WallpaperLayout::TILE:
        *out = wallpaper::WALLPAPER_LAYOUT_TILE;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // ASH_PUBLIC_INTERFACES_WALLPAPER_STRUCT_TRAITS_H_
