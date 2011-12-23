// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_TYPES_H_
#define ASH_LAUNCHER_LAUNCHER_TYPES_H_
#pragma once

#include <vector>

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura_shell/aura_shell_export.h"

namespace aura {
class Window;
}

namespace aura_shell {

// Type the LauncherItem represents.
enum AURA_SHELL_EXPORT LauncherItemType {
  TYPE_TABBED,
  TYPE_APP
};

// Represents an image in a launcher item of type TYPE_APP.
struct AURA_SHELL_EXPORT LauncherTabbedImage {
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

struct AURA_SHELL_EXPORT LauncherItem {
  LauncherItem() : type(TYPE_TABBED), window(NULL), user_data(NULL) {}
  LauncherItem(LauncherItemType type,
               aura::Window* window,
               void* user_data)
      : type(type),
        window(window),
        user_data(user_data) {}

  LauncherItemType type;
  aura::Window* window;
  void* user_data;

  // Image to display in the launcher if the item is of type TYPE_APP.
  SkBitmap app_image;

  // Image to display in the launcher if the item is of type TYPE_TABBED.
  LauncherTabbedImages tab_images;
};

typedef std::vector<LauncherItem> LauncherItems;

}  // namespace aura_shell

#endif  // ASH_LAUNCHER_LAUNCHER_TYPES_H_
