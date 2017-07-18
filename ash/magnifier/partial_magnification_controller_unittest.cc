// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Wrapper for PartialMagnificationController that exposes internal state to
// test functions.
class PartialMagnificationControllerTestApi {
 public:
  explicit PartialMagnificationControllerTestApi(
      PartialMagnificationController* controller)
      : controller_(controller) {}
  ~PartialMagnificationControllerTestApi() {}

  bool is_enabled() const { return controller_->is_enabled_; }
  bool is_active() const { return controller_->is_active_; }
  views::Widget* host_widget() const { return controller_->host_widget_; }

  gfx::Point GetWidgetOrigin() const {
    return host_widget()->GetWindowBoundsInScreen().origin();
  }

 private:
  PartialMagnificationController* controller_;

  DISALLOW_ASSIGN(PartialMagnificationControllerTestApi);
};

class PartialMagnificationControllerTest : public test::AshTestBase {
 public:
  PartialMagnificationControllerTest() {}
  ~PartialMagnificationControllerTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->display_manager()->UpdateDisplays();
  }

 protected:
  PartialMagnificationController* GetController() const {
    return Shell::Get()->partial_magnification_controller();
  }

  PartialMagnificationControllerTestApi GetTestApi() const {
    return PartialMagnificationControllerTestApi(
        Shell::Get()->partial_magnification_controller());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PartialMagnificationControllerTest);
};

// The magnifier should not show up immediately after being enabled.
TEST_F(PartialMagnificationControllerTest, InactiveByDefault) {
  GetController()->SetEnabled(true);
  EXPECT_FALSE(GetTestApi().is_active());
  EXPECT_FALSE(GetTestApi().host_widget());
}

// The magnifier should show up only after a pointer is pressed while enabled.
TEST_F(PartialMagnificationControllerTest, ActiveOnPointerDown) {
  GetEventGenerator().EnterPenPointerMode();

  // While disabled no magnifier shows up.
  GetEventGenerator().PressTouch();
  EXPECT_FALSE(GetTestApi().is_active());
  EXPECT_FALSE(GetTestApi().host_widget());
  GetEventGenerator().ReleaseTouch();

  // While enabled the magnifier is only active while the pointer is down.
  GetController()->SetEnabled(true);
  GetEventGenerator().PressTouch();
  EXPECT_TRUE(GetTestApi().is_active());
  EXPECT_TRUE(GetTestApi().host_widget());
  GetEventGenerator().ReleaseTouch();
  EXPECT_FALSE(GetTestApi().is_active());
  EXPECT_FALSE(GetTestApi().host_widget());
}

// Verifies that nothing bad happens if a second display is disconnected while
// the magnifier is active.
TEST_F(PartialMagnificationControllerTest, MultipleDisplays) {
  GetEventGenerator().EnterPenPointerMode();

  // Active magnifier with two displays, move it to the second display.
  UpdateDisplay("800x600,800x600");
  GetController()->SetEnabled(true);
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(1200, 300));
  EXPECT_TRUE(GetTestApi().is_active());
  EXPECT_TRUE(GetTestApi().host_widget());

  // Disconnect the second display, verify that magnifier is still active.
  UpdateDisplay("800x600");
  GetController()->SwitchTargetRootWindowIfNeeded(nullptr);
  EXPECT_TRUE(GetTestApi().is_active());
  EXPECT_TRUE(GetTestApi().host_widget());
}

// Turning the magnifier off while it is active destroys the window.
TEST_F(PartialMagnificationControllerTest, DisablingDisablesActive) {
  GetEventGenerator().EnterPenPointerMode();

  GetController()->SetEnabled(true);
  GetEventGenerator().PressTouch();
  EXPECT_TRUE(GetTestApi().is_active());

  GetController()->SetEnabled(false);
  EXPECT_FALSE(GetTestApi().is_active());
  EXPECT_FALSE(GetTestApi().host_widget());
}

// The magnifier only activates for pointer events.
TEST_F(PartialMagnificationControllerTest, ActivatesOnlyForPointer) {
  GetController()->SetEnabled(true);
  GetEventGenerator().PressTouch();
  EXPECT_FALSE(GetTestApi().is_active());
}

// The magnifier is always located at pointer.
TEST_F(PartialMagnificationControllerTest, MagnifierFollowsPointer) {
  GetEventGenerator().EnterPenPointerMode();
  GetController()->SetEnabled(true);

  // The window does not have to be centered on the press; compute the initial
  // window placement offset. Use a Vector2d for the + operator overload.
  GetEventGenerator().PressTouch();
  gfx::Vector2d offset(GetTestApi().GetWidgetOrigin().x(),
                       GetTestApi().GetWidgetOrigin().y());

  // Move the pointer around, make sure the window follows it.
  GetEventGenerator().MoveTouch(gfx::Point(32, 32));
  EXPECT_EQ(GetEventGenerator().current_location() + offset,
            GetTestApi().GetWidgetOrigin());

  GetEventGenerator().MoveTouch(gfx::Point(0, 10));
  EXPECT_EQ(GetEventGenerator().current_location() + offset,
            GetTestApi().GetWidgetOrigin());

  GetEventGenerator().MoveTouch(gfx::Point(10, 0));
  EXPECT_EQ(GetEventGenerator().current_location() + offset,
            GetTestApi().GetWidgetOrigin());

  GetEventGenerator().ReleaseTouch();

  // Make sure the window is initially placed correctly.
  GetEventGenerator().set_current_location(gfx::Point(50, 20));
  EXPECT_FALSE(GetTestApi().is_active());
  GetEventGenerator().PressTouch();
  EXPECT_EQ(GetEventGenerator().current_location() + offset,
            GetTestApi().GetWidgetOrigin());
}

}  // namespace ash
