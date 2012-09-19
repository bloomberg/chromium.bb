// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_tracker.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

namespace ash {
namespace test {

class DragDropTrackerTest : public test::AshTestBase {
 public:
  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    UpdateDisplay("0+0-200x200,0+201-200x200");
  }

  static aura::Window* CreateTestWindow(const gfx::Rect& bounds,
                                        aura::Window* parent) {
    static int window_id = 0;
    return aura::test::CreateTestWindowWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(),
        window_id++,
        bounds,
        parent);
  }

  static aura::Window* GetTarget(aura::RootWindow* root_window,
                          const gfx::Point& location) {
    scoped_ptr<internal::DragDropTracker> tracker(
        new internal::DragDropTracker(root_window));
    ui::MouseEvent e(ui::ET_MOUSE_DRAGGED,
                     location,
                     location,
                     ui::EF_NONE);
    aura::Window* target = tracker->GetTarget(e);
    return target;
  }

  static ui::MouseEvent* ConvertMouseEvent(aura::RootWindow* root_window,
                                           aura::Window* target,
                                           const ui::MouseEvent& event) {
    scoped_ptr<internal::DragDropTracker> tracker(
        new internal::DragDropTracker(root_window));
    ui::MouseEvent* converted = tracker->ConvertMouseEvent(target, event);
    return converted;
  }
};

// TODO(mazda): Remove this once ash/wm/coordinate_conversion.h supports
// non-X11 platforms.
#if defined(USE_X11)
#define MAYBE_GetTarget GetTarget
#else
#define MAYBE_GetTarget DISABLED_GetTarget
#endif

TEST_F(DragDropTrackerTest, MAYBE_GetTarget) {
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2U, root_windows.size());

  Shell::GetInstance()->set_active_root_window(root_windows[0]);

  scoped_ptr<aura::Window> window0(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100), NULL));
  window0->Show();

  Shell::GetInstance()->set_active_root_window(root_windows[1]);

  scoped_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(100, 100, 100, 100), NULL));
  window1->Show();

  // Start tracking from the RootWindow0 and check the point on RootWindow0 that
  // |window0| covers.
  EXPECT_EQ(window0.get(), GetTarget(root_windows[0], gfx::Point(50, 50)));

  // Start tracking from the RootWindow0 and check the point on RootWindow0 that
  // neither |window0| nor |window1| covers.
  EXPECT_NE(window0.get(), GetTarget(root_windows[0], gfx::Point(150, 150)));
  EXPECT_NE(window1.get(), GetTarget(root_windows[0], gfx::Point(150, 150)));

  // Start tracking from the RootWindow0 and check the point on RootWindow1 that
  // |window1| covers.
  EXPECT_EQ(window1.get(), GetTarget(root_windows[0], gfx::Point(150, 350)));

  // Start tracking from the RootWindow0 and check the point on RootWindow1 that
  // neither |window0| nor |window1| covers.
  EXPECT_NE(window0.get(), GetTarget(root_windows[0], gfx::Point(50, 250)));
  EXPECT_NE(window1.get(), GetTarget(root_windows[0], gfx::Point(50, 250)));

  // Start tracking from the RootWindow1 and check the point on RootWindow0 that
  // |window0| covers.
  EXPECT_EQ(window0.get(), GetTarget(root_windows[1], gfx::Point(50, -150)));

  // Start tracking from the RootWindow1 and check the point on RootWindow0 that
  // neither |window0| nor |window1| covers.
  EXPECT_NE(window0.get(), GetTarget(root_windows[1], gfx::Point(150, -50)));
  EXPECT_NE(window1.get(), GetTarget(root_windows[1], gfx::Point(150, -50)));

  // Start tracking from the RootWindow1 and check the point on RootWindow1 that
  // |window1| covers.
  EXPECT_EQ(window1.get(), GetTarget(root_windows[1], gfx::Point(150, 150)));

  // Start tracking from the RootWindow1 and check the point on RootWindow1 that
  // neither |window0| nor |window1| covers.
  EXPECT_NE(window0.get(), GetTarget(root_windows[1], gfx::Point(50, 50)));
  EXPECT_NE(window1.get(), GetTarget(root_windows[1], gfx::Point(50, 50)));
}

// TODO(mazda): Remove this once ash/wm/coordinate_conversion.h supports
// non-X11 platforms.
#if defined(USE_X11)
#define MAYBE_ConvertMouseEvent ConvertMouseEvent
#else
#define MAYBE_ConvertMouseEvent DISABLED_ConvertMouseEvent
#endif

TEST_F(DragDropTrackerTest, MAYBE_ConvertMouseEvent) {
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2U, root_windows.size());

  Shell::GetInstance()->set_active_root_window(root_windows[0]);
  scoped_ptr<aura::Window> window0(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100), NULL));
  window0->Show();

  Shell::GetInstance()->set_active_root_window(root_windows[1]);
  scoped_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(100, 100, 100, 100), NULL));
  window1->Show();

  // Start tracking from the RootWindow0 and converts the mouse event into
  // |window0|'s coodinates.
  ui::MouseEvent original00(ui::ET_MOUSE_DRAGGED,
                            gfx::Point(50, 50),
                            gfx::Point(50, 50),
                            ui::EF_NONE);
  scoped_ptr<ui::MouseEvent> converted00(ConvertMouseEvent(root_windows[0],
                                                           window0.get(),
                                                           original00));
  EXPECT_EQ(original00.type(), converted00->type());
  EXPECT_EQ(gfx::Point(50, 50), converted00->location());
  EXPECT_EQ(gfx::Point(50, 50), converted00->root_location());
  EXPECT_EQ(original00.flags(), converted00->flags());

  // Start tracking from the RootWindow0 and converts the mouse event into
  // |window1|'s coodinates.
  ui::MouseEvent original01(ui::ET_MOUSE_DRAGGED,
                            gfx::Point(150, 350),
                            gfx::Point(150, 350),
                            ui::EF_NONE);
  scoped_ptr<ui::MouseEvent> converted01(ConvertMouseEvent(root_windows[0],
                                                           window1.get(),
                                                           original01));
  EXPECT_EQ(original01.type(), converted01->type());
  EXPECT_EQ(gfx::Point(50, 50), converted01->location());
  EXPECT_EQ(gfx::Point(150, 150), converted01->root_location());
  EXPECT_EQ(original01.flags(), converted01->flags());

  // Start tracking from the RootWindow1 and converts the mouse event into
  // |window0|'s coodinates.
  ui::MouseEvent original10(ui::ET_MOUSE_DRAGGED,
                            gfx::Point(50, -150),
                            gfx::Point(50, -150),
                            ui::EF_NONE);
  scoped_ptr<ui::MouseEvent> converted10(ConvertMouseEvent(root_windows[1],
                                                           window0.get(),
                                                           original10));
  EXPECT_EQ(original10.type(), converted10->type());
  EXPECT_EQ(gfx::Point(50, 50), converted10->location());
  EXPECT_EQ(gfx::Point(50, 50), converted10->root_location());
  EXPECT_EQ(original10.flags(), converted10->flags());

  // Start tracking from the RootWindow1 and converts the mouse event into
  // |window1|'s coodinates.
  ui::MouseEvent original11(ui::ET_MOUSE_DRAGGED,
                            gfx::Point(150, 150),
                            gfx::Point(150, 150),
                            ui::EF_NONE);
  scoped_ptr<ui::MouseEvent> converted11(ConvertMouseEvent(root_windows[1],
                                                           window1.get(),
                                                           original11));
  EXPECT_EQ(original11.type(), converted11->type());
  EXPECT_EQ(gfx::Point(50, 50), converted11->location());
  EXPECT_EQ(gfx::Point(150, 150), converted11->root_location());
  EXPECT_EQ(original11.flags(), converted11->flags());
}

}  // namespace test
}  // namespace aura
