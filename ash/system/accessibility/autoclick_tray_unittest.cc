// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_tray.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/shell.h"
#include "ash/system/accessibility/autoclick_tray_action_list_view.h"
#include "ash/system/accessibility/autoclick_tray_action_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/event.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

const int kActionsListId = 1;
const int kLeftClickViewID = 2;
const int kRightClickViewID = 3;
const int kDoubleClickViewID = 4;
const int kDragAndDropViewID = 5;
const int kPauseViewID = 6;

AutoclickTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()->autoclick_tray();
}

ui::GestureEvent CreateTapEvent() {
  return ui::GestureEvent(0, 0, 0, base::TimeTicks(),
                          ui::GestureEventDetails(ui::ET_GESTURE_TAP));
}

}  // namespace

class AutoclickTrayTest : public AshTestBase {
 public:
  AutoclickTrayTest() = default;
  ~AutoclickTrayTest() override = default;

  // testing::Test:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalAccessibilityAutoclick);
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->SetAutoclickEnabled(true);
  }

 protected:
  // Returns true if the Autoclick tray is visible.
  bool IsVisible() { return GetTray()->visible(); }

  // Returns true if the background color of the tray is active.
  bool IsTrayBackgroundActive() { return GetTray()->is_active(); }

  // Gets the current tray image icon.
  gfx::ImageSkia* GetImageIcon() { return &GetTray()->tray_image_; }

  TrayBubbleWrapper* GetBubble() { return GetTray()->bubble_.get(); }

  AutoclickTrayActionView* GetTrayActionView(int view_id) {
    TrayBubbleWrapper* bubble = GetBubble();
    if (!bubble)
      return nullptr;
    AutoclickTrayActionListView* actions_list =
        static_cast<AutoclickTrayActionListView*>(
            bubble->bubble_view()->GetViewByID(kActionsListId));
    if (!actions_list)
      return nullptr;
    return static_cast<AutoclickTrayActionView*>(
        actions_list->GetViewByID(view_id));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoclickTrayTest);
};

// Ensures that creation doesn't cause any crashes and adds the image icon.
// Also checks that the tray is visible.
TEST_F(AutoclickTrayTest, BasicConstruction) {
  EXPECT_TRUE(GetImageIcon());
  EXPECT_TRUE(IsVisible());
}

// Tests the icon disappears when autoclick is disabled and re-appears
// when it is enabled.
TEST_F(AutoclickTrayTest, ShowsAndHidesWithAutoclickEnabled) {
  Shell::Get()->accessibility_controller()->SetAutoclickEnabled(false);
  EXPECT_FALSE(IsVisible());
  Shell::Get()->accessibility_controller()->SetAutoclickEnabled(true);
  EXPECT_TRUE(IsVisible());
}

// Test that clicking the button opens the bubble.
TEST_F(AutoclickTrayTest, ButtonOpensBubble) {
  EXPECT_FALSE(IsTrayBackgroundActive());
  EXPECT_FALSE(GetBubble());

  GetTray()->PerformAction(CreateTapEvent());
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(GetBubble());

  GetTray()->PerformAction(CreateTapEvent());
  EXPECT_FALSE(IsTrayBackgroundActive());
  EXPECT_FALSE(GetBubble());
}

TEST_F(AutoclickTrayTest, CanSelectAutoclickTypeFromBubble) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  // Set to a different event type than the first event in kTestCases.
  controller->SetAutoclickEventType(mojom::AutoclickEventType::kRightClick);

  const struct {
    int view_id;
    mojom::AutoclickEventType expected_event_type;
  } kTestCases[] = {
      {kLeftClickViewID, mojom::AutoclickEventType::kLeftClick},
      {kRightClickViewID, mojom::AutoclickEventType::kRightClick},
      {kDoubleClickViewID, mojom::AutoclickEventType::kDoubleClick},
      {kDragAndDropViewID, mojom::AutoclickEventType::kDragAndDrop},
      {kPauseViewID, mojom::AutoclickEventType::kNoAction},
  };

  for (const auto& test : kTestCases) {
    // Open the menu.
    GetTray()->PerformAction(CreateTapEvent());

    // Find the autoclick action button for this test case.
    AutoclickTrayActionView* view = GetTrayActionView(test.view_id);
    ASSERT_TRUE(view) << "No view for id " << test.view_id;

    // Tap the left-click action button.
    ui::GestureEvent event = CreateTapEvent();
    view->OnGestureEvent(&event);

    // Bubble is hidden.
    EXPECT_FALSE(GetBubble());

    // Pref change happened.
    EXPECT_EQ(test.expected_event_type, controller->GetAutoclickEventType());
  }
}

}  // namespace ash
