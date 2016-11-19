// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_details_view.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/special_popup_row.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_header_button.h"
#include "ash/common/system/tray/view_click_listener.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

namespace {

class TestDetailsView : public TrayDetailsView {
 public:
  explicit TestDetailsView(SystemTrayItem* owner) : TrayDetailsView(owner) {
    // Uses bluetooth label for testing purpose. It can be changed to any
    // string_id.
    CreateTitleRow(IDS_ASH_STATUS_TRAY_BLUETOOTH);
  }

  ~TestDetailsView() override {}

  TrayPopupHeaderButton* tray_popup_header_button() {
    return tray_popup_header_button_;
  }

  void FocusTitleRow() { title_row()->content()->RequestFocus(); }

  // TrayDetailsView:
  void CreateExtraTitleRowButtons() override {
    // TODO(tdanderson): Add test coverage for material design buttons in the
    // title row once they are implemented.
    if (MaterialDesignController::IsSystemTrayMenuMaterial())
      return;

    tray_popup_header_button_ =
        new TrayPopupHeaderButton(this, IDR_AURA_UBER_TRAY_BLUETOOTH_ENABLED,
                                  IDR_AURA_UBER_TRAY_BLUETOOTH_DISABLED,
                                  IDR_AURA_UBER_TRAY_BLUETOOTH_ENABLED_HOVER,
                                  IDR_AURA_UBER_TRAY_BLUETOOTH_DISABLED_HOVER,
                                  IDS_ASH_STATUS_TRAY_BLUETOOTH);
    title_row()->AddViewToRowNonMd(tray_popup_header_button_, true);
  }

 private:
  TrayPopupHeaderButton* tray_popup_header_button_;

  DISALLOW_COPY_AND_ASSIGN(TestDetailsView);
};

// Trivial item implementation that tracks its views for testing.
class TestItem : public SystemTrayItem {
 public:
  TestItem()
      : SystemTrayItem(AshTestBase::GetPrimarySystemTray(), UMA_TEST),
        tray_view_(nullptr),
        default_view_(nullptr),
        detailed_view_(nullptr) {}

  // Overridden from SystemTrayItem:
  views::View* CreateTrayView(LoginStatus status) override {
    tray_view_ = new views::View;
    return tray_view_;
  }
  views::View* CreateDefaultView(LoginStatus status) override {
    default_view_ = new views::View;
    default_view_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    return default_view_;
  }
  views::View* CreateDetailedView(LoginStatus status) override {
    detailed_view_ = new TestDetailsView(this);
    return detailed_view_;
  }
  void DestroyTrayView() override { tray_view_ = NULL; }
  void DestroyDefaultView() override { default_view_ = NULL; }
  void DestroyDetailedView() override { detailed_view_ = NULL; }

  views::View* tray_view() const { return tray_view_; }
  views::View* default_view() const { return default_view_; }
  TestDetailsView* detailed_view() const { return detailed_view_; }

 private:
  views::View* tray_view_;
  views::View* default_view_;
  TestDetailsView* detailed_view_;

  DISALLOW_COPY_AND_ASSIGN(TestItem);
};

}  // namespace

class TrayDetailsViewTest : public AshTestBase {
 public:
  TrayDetailsViewTest() {}
  ~TrayDetailsViewTest() override {}

  HoverHighlightView* CreateAndShowHoverHighlightView() {
    SystemTray* tray = GetPrimarySystemTray();
    TestItem* test_item = new TestItem;
    tray->AddTrayItem(test_item);
    tray->ShowDefaultView(BUBBLE_CREATE_NEW);
    RunAllPendingInMessageLoop();
    tray->ShowDetailedView(test_item, 0, true, BUBBLE_USE_EXISTING);
    RunAllPendingInMessageLoop();

    return static_cast<HoverHighlightView*>(
        test_item->detailed_view()->title_row()->content());
  }

  TrayPopupHeaderButton* CreateAndShowTrayPopupHeaderButton() {
    SystemTray* tray = GetPrimarySystemTray();
    TestItem* test_item = new TestItem;
    tray->AddTrayItem(test_item);
    tray->ShowDefaultView(BUBBLE_CREATE_NEW);
    RunAllPendingInMessageLoop();
    tray->ShowDetailedView(test_item, 0, true, BUBBLE_USE_EXISTING);
    RunAllPendingInMessageLoop();

    return test_item->detailed_view()->tray_popup_header_button();
  }

  void TransitionFromDetailedToDefaultView(TestDetailsView* detailed) {
    detailed->TransitionToDefaultView();
    scoped_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(
        GetTrayConstant(TRAY_POPUP_TRANSITION_TO_DEFAULT_DELAY)));
  }

  void FocusBackButton(TestDetailsView* detailed) {
    detailed->back_button_->RequestFocus();
  }

 private:
  // Used to control the |transition_delay_timer_|.
  base::ScopedMockTimeMessageLoopTaskRunner scoped_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(TrayDetailsViewTest);
};

TEST_F(TrayDetailsViewTest, TransitionToDefaultViewTest) {
  SystemTray* tray = GetPrimarySystemTray();
  ASSERT_TRUE(tray->GetWidget());

  TestItem* test_item_1 = new TestItem;
  TestItem* test_item_2 = new TestItem;
  tray->AddTrayItem(test_item_1);
  tray->AddTrayItem(test_item_2);

  // Ensure the tray views are created.
  ASSERT_TRUE(test_item_1->tray_view() != NULL);
  ASSERT_TRUE(test_item_2->tray_view() != NULL);

  // Show the default view.
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();

  // Show the detailed view of item 2.
  tray->ShowDetailedView(test_item_2, 0, true, BUBBLE_USE_EXISTING);
  EXPECT_TRUE(test_item_2->detailed_view());
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(test_item_2->default_view());

  // Transition back to default view, the default view of item 2 should have
  // focus.
  tray->GetSystemBubble()->bubble_view()->set_can_activate(true);
  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    FocusBackButton(test_item_2->detailed_view());
  else
    test_item_2->detailed_view()->FocusTitleRow();
  TransitionFromDetailedToDefaultView(test_item_2->detailed_view());
  RunAllPendingInMessageLoop();

  EXPECT_TRUE(test_item_2->default_view());
  EXPECT_FALSE(test_item_2->detailed_view());
  EXPECT_TRUE(test_item_2->default_view()->HasFocus());

  // Show the detailed view of item 2 again.
  tray->ShowDetailedView(test_item_2, 0, true, BUBBLE_USE_EXISTING);
  EXPECT_TRUE(test_item_2->detailed_view());
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(test_item_2->default_view());

  // Transition back to default view, the default view of item 2 should NOT have
  // focus.
  TransitionFromDetailedToDefaultView(test_item_2->detailed_view());
  RunAllPendingInMessageLoop();

  EXPECT_TRUE(test_item_2->default_view());
  EXPECT_FALSE(test_item_2->detailed_view());
  EXPECT_FALSE(test_item_2->default_view()->HasFocus());
}

// Tests that HoverHighlightView enters hover state in response to touch.
TEST_F(TrayDetailsViewTest, HoverHighlightViewTouchFeedback) {
  // Material design detailed views will not use the visual feedback from
  // HoverHighlightView.
  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    return;

  HoverHighlightView* view = CreateAndShowHoverHighlightView();
  EXPECT_FALSE(view->hover());

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.set_current_location(view->GetBoundsInScreen().CenterPoint());
  generator.PressTouch();
  EXPECT_TRUE(view->hover());

  generator.ReleaseTouch();
}

// Tests that touch events leaving HoverHighlightView cancel the hover state.
TEST_F(TrayDetailsViewTest, HoverHighlightViewTouchFeedbackCancellation) {
  // Material design detailed views will not use the visual feedback from
  // HoverHighlightView.
  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    return;

  HoverHighlightView* view = CreateAndShowHoverHighlightView();
  EXPECT_FALSE(view->hover());

  gfx::Rect view_bounds = view->GetBoundsInScreen();
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.set_current_location(view_bounds.CenterPoint());
  generator.PressTouch();
  EXPECT_TRUE(view->hover());

  gfx::Point move_point(view_bounds.x(), view_bounds.CenterPoint().y());
  generator.MoveTouch(move_point);
  EXPECT_FALSE(view->hover());

  generator.set_current_location(move_point);
  generator.ReleaseTouch();
  EXPECT_FALSE(view->hover());
}

// Tests that TrayPopupHeaderButton renders a background in response to touch.
TEST_F(TrayDetailsViewTest, TrayPopupHeaderButtonTouchFeedback) {
  // Material design detailed views will not use TrayPopupHeaderButton.
  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    return;

  TrayPopupHeaderButton* button = CreateAndShowTrayPopupHeaderButton();
  EXPECT_FALSE(button->background());

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.set_current_location(button->GetBoundsInScreen().CenterPoint());
  generator.PressTouch();
  EXPECT_TRUE(button->background());

  generator.ReleaseTouch();
  EXPECT_FALSE(button->background());
}

// Tests that touch events leaving TrayPopupHeaderButton cancel the touch
// feedback background.
TEST_F(TrayDetailsViewTest, TrayPopupHeaderButtonTouchFeedbackCancellation) {
  // Material design detailed views will not use TrayPopupHeaderButton.
  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    return;

  TrayPopupHeaderButton* button = CreateAndShowTrayPopupHeaderButton();
  EXPECT_FALSE(button->background());

  gfx::Rect view_bounds = button->GetBoundsInScreen();
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.set_current_location(view_bounds.CenterPoint());
  generator.PressTouch();
  EXPECT_TRUE(button->background());

  gfx::Point move_point(view_bounds.x(), view_bounds.CenterPoint().y());
  generator.MoveTouch(move_point);
  EXPECT_FALSE(button->background());

  generator.set_current_location(move_point);
  generator.ReleaseTouch();
  EXPECT_FALSE(button->background());
}

// Tests that a mouse entering TrayPopupHeaderButton renders a background as
// visual feedback.
TEST_F(TrayDetailsViewTest, TrayPopupHeaderButtonMouseHoverFeedback) {
  // Material design detailed views will not use TrayPopupHeaderButton.
  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    return;

  TrayPopupHeaderButton* button = CreateAndShowTrayPopupHeaderButton();
  EXPECT_FALSE(button->background());

  ui::test::EventGenerator& generator = GetEventGenerator();
  gfx::Rect bounds = button->GetBoundsInScreen();
  gfx::Point initial_point(bounds.x() - 1, bounds.y());
  generator.set_current_location(initial_point);
  generator.MoveMouseBy(1, 0);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(button->background());
}

}  // namespace test
}  // namespace ash
