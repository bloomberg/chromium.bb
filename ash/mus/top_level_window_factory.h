// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_TOP_LEVEL_WINDOW_FACTORY_H_
#define ASH_MUS_TOP_LEVEL_WINDOW_FACTORY_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

namespace aura {
class Window;
}

namespace ui {
namespace mojom {
enum class WindowType;
}
}

namespace ash {
namespace mus {

class WindowManager;

// Creates and parents a new top-level window and returns it. The returned
// aura::Window is owned by its parent.
aura::Window* CreateAndParentTopLevelWindow(
    WindowManager* window_manager,
    ui::mojom::WindowType window_type,
    std::map<std::string, std::vector<uint8_t>>* properties);

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_TOP_LEVEL_WINDOW_FACTORY_H_
