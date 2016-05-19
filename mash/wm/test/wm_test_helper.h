// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_TEST_WM_TEST_HELPER_H_
#define MASH_WM_TEST_WM_TEST_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "components/mus/public/cpp/tests/test_window_tree_connection_setup.h"
#include "mash/wm/window_manager_application.h"

namespace base {
class MessageLoop;
}

namespace gfx {
class Rect;
}

namespace mus {
class Window;
}

namespace mash {
namespace wm {

class WmTestScreen;

// WMTestHelper is responsible for configuring a WindowTreeConnection that
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
  mus::TestWindowTreeConnectionSetup window_tree_connection_setup_;
  WindowManagerApplication window_manager_app_;
  WmTestScreen* screen_ = nullptr;  // Owned by |window_manager_app_|.

  DISALLOW_COPY_AND_ASSIGN(WmTestHelper);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_TEST_WM_TEST_HELPER_H_
