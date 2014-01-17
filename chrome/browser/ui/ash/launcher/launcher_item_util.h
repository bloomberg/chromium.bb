// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_ITEM_UTIL_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_ITEM_UTIL_H_

#include "base/strings/string16.h"

namespace aura {
class Window;
}

// Creates an item on the shelf for |window|. The item is destroyed when the
// window is destroyed.
// TODO(simonhong): Move this function to NativeWidgetAura.
void CreateShelfItemForDialog(int image_resource_id,
                              aura::Window* window);

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_ITEM_UTIL_H_
