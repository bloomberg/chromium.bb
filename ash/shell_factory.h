// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_FACTORY_H_
#define ASH_SHELL_FACTORY_H_

#include "ash/ash_export.h"

namespace aura {
class RootWindow;
}

namespace gfx {
class ImageSkia;
}

namespace views {
class View;
class Widget;
}

// Declarations of shell component factory functions.

namespace ash {

views::Widget* CreateDesktopBackground(aura::Window* root_window,
                                       int container_id);

ASH_EXPORT views::Widget* CreateStatusArea(views::View* contents);

}  // namespace ash


#endif  // ASH_SHELL_FACTORY_H_
