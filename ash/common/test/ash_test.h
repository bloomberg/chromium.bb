// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_TEST_ASH_TEST_H_
#define ASH_COMMON_TEST_ASH_TEST_H_

#include <memory>
#include <string>

#include "ash/public/cpp/shell_window_ids.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_layout.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/window_types.h"

namespace display {
class Display;
}

namespace views {
class WidgetDelegate;
}

namespace ash {

class AshTestImpl;
class SystemTray;
class WmShelf;
class WmWindow;

namespace test {
class TestSystemTrayDelegate;
}

// Wraps a WmWindow calling WmWindow::Destroy() from the destructor. WmWindow is
// owned by the corresponding window implementation. The only way to delete
// WmWindow is to call WmWindow::Destroy(), which deletes the corresponding
// window, then the WmWindow. This class calls WmWindow::Destroy() from its
// destructor.
class WindowOwner {
 public:
  explicit WindowOwner(WmWindow* window);
  ~WindowOwner();

  WmWindow* window() { return window_; }

 private:
  WmWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(WindowOwner);
};

// Base class for ash tests. This class calls through to AshTestImpl for the
// real implementation. This class exists so that tests can be written to
// ash/common and run in both mus and aura.
//
// The implementation of AshTestImpl that is used depends upon gn targets. To
// use the aura backend depend on "//ash/test:ash_with_aura_test_support." The
// mus backend is not provided as a separate link target.
class AshTest : public testing::Test {
 public:
  AshTest();
  ~AshTest() override;

  // Returns the WmShelf for the primary display.
  static WmShelf* GetPrimaryShelf();

  // Returns the system tray on the primary display.
  static SystemTray* GetPrimarySystemTray();

  static test::TestSystemTrayDelegate* GetSystemTrayDelegate();

  bool SupportsMultipleDisplays() const;

  // Update the display configuration as given in |display_spec|.
  // See test::DisplayManagerTestApi::UpdateDisplay for more details.
  void UpdateDisplay(const std::string& display_spec);

  // Creates a visible window in the appropriate container. If
  // |bounds_in_screen| is empty the window is added to the primary root
  // window, otherwise the window is added to the display matching
  // |bounds_in_screen|. |shell_window_id| is the shell window id to give to
  // the new window.
  std::unique_ptr<WindowOwner> CreateTestWindow(
      const gfx::Rect& bounds_in_screen = gfx::Rect(),
      ui::wm::WindowType type = ui::wm::WINDOW_TYPE_NORMAL,
      int shell_window_id = kShellWindowId_Invalid);

  // Creates a visible top-level window. For aura a top-level window is a Window
  // that has a delegate, see aura::Window::GetToplevelWindow() for more
  // details.
  std::unique_ptr<WindowOwner> CreateToplevelTestWindow(
      const gfx::Rect& bounds_in_screen = gfx::Rect(),
      int shell_window_id = kShellWindowId_Invalid);

  // Creates a visible window parented to |parent| with the specified bounds and
  // id.
  std::unique_ptr<WindowOwner> CreateChildWindow(
      WmWindow* parent,
      const gfx::Rect& bounds = gfx::Rect(),
      int shell_window_id = kShellWindowId_Invalid);

  // Creates and shows a widget. See ash/public/cpp/shell_window_ids.h for
  // values for |container_id|.
  static std::unique_ptr<views::Widget> CreateTestWidget(
      const gfx::Rect& bounds,
      views::WidgetDelegate* delegate = nullptr,
      int container_id = kShellWindowId_DefaultContainer);

  // Returns the Display for the secondary display. It's assumed there are two
  // displays.
  display::Display GetSecondaryDisplay();

  // Sets the placement of the secondary display. Returns true if the secondary
  // display can be moved, false otherwise. The false return value is temporary
  // until mus fully supports this.
  bool SetSecondaryDisplayPlacement(
      display::DisplayPlacement::Position position,
      int offset);

  // Configures |init_params| so that the widget will be created on the same
  // display as |window|.
  void ConfigureWidgetInitParamsForDisplay(
      WmWindow* window,
      views::Widget::InitParams* init_params);

  // Adds |window| to the appropriate container in the primary root window.
  void ParentWindowInPrimaryRootWindow(WmWindow* window);

  // Adds |window| as as a transient child of |parent|.
  void AddTransientChild(WmWindow* parent, WmWindow* window);

  void RunAllPendingInMessageLoop();

 protected:
  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<AshTestImpl> test_impl_;

  DISALLOW_COPY_AND_ASSIGN(AshTest);
};

}  // namespace ash

#endif  // ASH_COMMON_TEST_ASH_TEST_H_
