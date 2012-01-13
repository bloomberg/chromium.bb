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

struct ASH_EXPORT LauncherItem {
  LauncherItem();
  LauncherItem(LauncherItemType type, aura::Window* window);
  ~LauncherItem();

  LauncherItemType type;
  aura::Window* window;

  // Number of tabs. Only used if this is TYPE_TABBED.
  int num_tabs;

  // Image to display in the launcher. If this item is TYPE_TABBED the image is
  // a favicon image.
  SkBitmap image;
};

typedef std::vector<LauncherItem> LauncherItems;

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_TYPES_H_
