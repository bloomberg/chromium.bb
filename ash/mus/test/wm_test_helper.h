// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_TEST_WM_TEST_HELPER_H_
#define ASH_MUS_TEST_WM_TEST_HELPER_H_

#include <memory>

#include "ash/mus/window_manager_application.h"
#include "base/macros.h"
#include "services/ui/public/cpp/tests/test_window_tree_client_setup.h"

namespace base {
class MessageLoop;
}

namespace gfx {
class Rect;
}

namespace ui {
class Window;
}

namespace ash {
namespace mus {

class WmTestScreen;

// WMTestHelper is responsible for configuring a WindowTreeClient that
// does not talk to mus. Additionally a test Screen (WmTestScreen) is created.
class WmTestHelper {
 public:
  WmTestHelper();
  ~WmTestHelper();

  void Init();

  WindowManagerApplication* window_manager_app() {
    return &window_manager_app_;
  }

  WmTestScreen* screen() { return screen_; }

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;
  ::ui::TestWindowTreeClientSetup window_tree_client_setup_;
  WindowManagerApplication window_manager_app_;
  WmTestScreen* screen_ = nullptr;  // Owned by |window_manager_app_|.

  DISALLOW_COPY_AND_ASSIGN(WmTestHelper);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_TEST_WM_TEST_HELPER_H_
