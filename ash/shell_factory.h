// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_FACTORY_H_
#define ASH_SHELL_FACTORY_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/desktop_background/desktop_background_resources.h"

class SkBitmap;

namespace views {
class View;
class Widget;
}

// Declarations of shell component factory functions.

namespace ash {

namespace internal {
void CreateDesktopBackground(const SkBitmap& wallpaper,
                             WallpaperLayout wallpaper_layout);
ASH_EXPORT views::Widget* CreateStatusArea(views::View* contents);
}  // namespace internal

}  // namespace ash


#endif  // ASH_SHELL_FACTORY_H_
