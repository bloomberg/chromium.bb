// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_STRUCT_TRAITS_H_
#define ASH_PUBLIC_CPP_WALLPAPER_STRUCT_TRAITS_H_

#include "ash/public/interfaces/wallpaper.mojom.h"
#include "components/wallpaper/wallpaper_info.h"

namespace base {
class Time;
}  // namespace base

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

template <>
struct EnumTraits<ash::mojom::WallpaperType,
                  user_manager::User::WallpaperType> {
  static ash::mojom::WallpaperType ToMojom(
      user_manager::User::WallpaperType input) {
    switch (input) {
      case user_manager::User::WallpaperType::DAILY:
        return ash::mojom::WallpaperType::DAILY;
      case user_manager::User::CUSTOMIZED:
        return ash::mojom::WallpaperType::CUSTOMIZED;
      case user_manager::User::DEFAULT:
        return ash::mojom::WallpaperType::DEFAULT;
      case user_manager::User::ONLINE:
        return ash::mojom::WallpaperType::ONLINE;
      case user_manager::User::POLICY:
        return ash::mojom::WallpaperType::POLICY;
      case user_manager::User::THIRDPARTY:
        return ash::mojom::WallpaperType::THIRDPARTY;
      case user_manager::User::DEVICE:
        return ash::mojom::WallpaperType::DEVICE;
      case user_manager::User::WALLPAPER_TYPE_COUNT:
        break;
    }
    NOTREACHED();
    return ash::mojom::WallpaperType::DAILY;
  }

  static bool FromMojom(ash::mojom::WallpaperType input,
                        user_manager::User::WallpaperType* out) {
    switch (input) {
      case ash::mojom::WallpaperType::DAILY:
        *out = user_manager::User::WallpaperType::DAILY;
        return true;
      case ash::mojom::WallpaperType::CUSTOMIZED:
        *out = user_manager::User::WallpaperType::CUSTOMIZED;
        return true;
      case ash::mojom::WallpaperType::DEFAULT:
        *out = user_manager::User::WallpaperType::DEFAULT;
        return true;
      case ash::mojom::WallpaperType::ONLINE:
        *out = user_manager::User::WallpaperType::ONLINE;
        return true;
      case ash::mojom::WallpaperType::POLICY:
        *out = user_manager::User::WallpaperType::POLICY;
        return true;
      case ash::mojom::WallpaperType::THIRDPARTY:
        *out = user_manager::User::WallpaperType::THIRDPARTY;
        return true;
      case ash::mojom::WallpaperType::DEVICE:
        *out = user_manager::User::WallpaperType::DEVICE;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct ASH_PUBLIC_EXPORT
    StructTraits<ash::mojom::WallpaperInfoDataView, wallpaper::WallpaperInfo> {
  static const std::string& location(const wallpaper::WallpaperInfo& i) {
    return i.location;
  }
  static const wallpaper::WallpaperLayout& layout(
      const wallpaper::WallpaperInfo& i) {
    return i.layout;
  }
  static const user_manager::User::WallpaperType& type(
      const wallpaper::WallpaperInfo& i) {
    return i.type;
  }
  static const base::Time& date(const wallpaper::WallpaperInfo& i) {
    return i.date;
  }
  static bool Read(ash::mojom::WallpaperInfoDataView data,
                   wallpaper::WallpaperInfo* out);
};

}  // namespace mojo

#endif  // ASH_PUBLIC_CPP_WALLPAPER_STRUCT_TRAITS_H_
