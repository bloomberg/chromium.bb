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
class SequencedWorkerPoolOwner;
}

namespace ui {
class WindowTreeClientPrivate;
}

namespace views {
class ViewsDelegate;
}

namespace ash {
namespace mus {

class RootWindowController;

// WMTestHelper is responsible for configuring a WindowTreeClient that
// does not talk to mus.
class WmTestHelper {
 public:
  WmTestHelper();
  ~WmTestHelper();

  void Init();

  WindowManagerApplication* window_manager_app() {
    return window_manager_app_.get();
  }

  // Returns the RootWindowControllers ordered by display id (which we assume
  // correlates with creation order).
  std::vector<RootWindowController*> GetRootsOrderedByDisplayId();

  void UpdateDisplay(const std::string& display_spec);

 private:
  // Creates a new RootWindowController given |display_spec|, which is the
  // configuration of the display. On entry |next_x| is the x-coordinate to
  // place the display at, on exit |next_x| is set to the x-coordinate to place
  // the next display at.
  RootWindowController* CreateRootWindowController(
      const std::string& display_spec,
      int* next_x);

  // Updates the display of an existing RootWindowController. See
  // CreateRootWindowController() for details on |next_x|.
  void UpdateDisplay(RootWindowController* root_window_controller,
                     const std::string& display_spec,
                     int* next_x);

  // Destroys a RootWindowController.
  void DestroyRootWindowController(
      RootWindowController* root_window_controller);

  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<views::ViewsDelegate> views_delegate_;
  ui::TestWindowTreeClientSetup window_tree_client_setup_;
  std::unique_ptr<WindowManagerApplication> window_manager_app_;
  std::unique_ptr<ui::WindowTreeClientPrivate> window_tree_client_private_;

  // Id for the next Display created by CreateRootWindowController().
  int64_t next_display_id_ = 1;

  std::unique_ptr<base::SequencedWorkerPoolOwner> blocking_pool_owner_;

  DISALLOW_COPY_AND_ASSIGN(WmTestHelper);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_TEST_WM_TEST_HELPER_H_
