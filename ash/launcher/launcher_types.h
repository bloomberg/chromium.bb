// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_TYPES_H_
#define ASH_LAUNCHER_LAUNCHER_TYPES_H_
#pragma once

#include <vector>

#include "third_party/skia/include/core/SkBitmap.h"
#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace ash {

// Type the LauncherItem represents.
enum ASH_EXPORT LauncherItemType {
  TYPE_TABBED,
  TYPE_APP
};

// Represents an image in a launcher item of type TYPE_APP.
struct ASH_EXPORT LauncherTabbedImage {
  LauncherTabbedImage() : user_data(NULL) {}
  LauncherTabbedImage(const SkBitmap& image, void* user_data)
      : image(image),
        user_data(user_data) {
  }

  // The image to show.
  SkBitmap image;

  // Used to identify the image.
  void* user_data;
};

typedef std::vector<LauncherTabbedImage> LauncherTabbedImages;

struct ASH_EXPORT LauncherItem {
  LauncherItem();
  LauncherItem(LauncherItemType type,
               aura::Window* window,
               void* user_data);
  ~LauncherItem();

  LauncherItemType type;
  aura::Window* window;
  void* user_data;

  // Image to display in the launcher if the item is of type TYPE_APP.
  SkBitmap app_image;

  // Image to display in the launcher if the item is of type TYPE_TABBED.
  LauncherTabbedImages tab_images;
};

typedef std::vector<LauncherItem> LauncherItems;

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_TYPES_H_
