// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/views/scoped_macviews_browser_mode.h"
#include "components/autofill/core/browser/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

using testing::Return;
using testing::ReturnRef;

class MockAutofillPopupViewDelegate : public AutofillPopupViewDelegate {
 public:
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD0(ViewDestroyed, void());
  MOCK_METHOD1(SetSelectionAtPoint, void(const gfx::Point&));
  MOCK_METHOD0(AcceptSelectedLine, bool());
  MOCK_METHOD0(SelectionCleared, void());
  MOCK_CONST_METHOD0(HasSelection, bool());

  // TODO(jdduke): Mock this method upon resolution of crbug.com/352463.
  MOCK_CONST_METHOD0(popup_bounds, gfx::Rect());
  MOCK_METHOD0(container_view, gfx::NativeView());
  MOCK_CONST_METHOD0(element_bounds, gfx::RectF&());
  MOCK_CONST_METHOD0(IsRTL, bool());
  MOCK_METHOD0(GetSuggestions, const std::vector<autofill::Suggestion>());
#if !defined(OS_ANDROID)
  MOCK_METHOD1(SetTypesetter, void(gfx::Typesetter typesetter));
  MOCK_METHOD1(GetElidedValueWidthForRow, int(int));
  MOCK_METHOD1(GetElidedLabelWidthForRow, int(int));
#endif
};

}  // namespace

class AutofillPopupBaseViewTest : public InProcessBrowserTest {
 public:
  AutofillPopupBaseViewTest() {}
  ~AutofillPopupBaseViewTest() override {}

  void SetUpOnMainThread() override {
    gfx::NativeView native_view =
        browser()->tab_strip_model()->GetActiveWebContents()->GetNativeView();
    EXPECT_CALL(mock_delegate_, container_view())
        .WillRepeatedly(Return(native_view));
    EXPECT_CALL(mock_delegate_, ViewDestroyed());

    view_ = new AutofillPopupBaseView(
        &mock_delegate_, views::Widget::GetWidgetForNativeWindow(
                             browser()->window()->GetNativeWindow()));
  }

  void ShowView() {
    view_->DoShow();
  }

  ui::GestureEvent CreateGestureEvent(ui::EventType type, gfx::Point point) {
    return ui::GestureEvent(point.x(),
                            point.y(),
                            0,
                            ui::EventTimeForNow(),
                            ui::GestureEventDetails(type));
  }

  void SimulateGesture(ui::GestureEvent* event) {
    view_->OnGestureEvent(event);
  }

 protected:
  test::ScopedMacViewsBrowserMode views_mode_{true};
  testing::NiceMock<MockAutofillPopupViewDelegate> mock_delegate_;
  AutofillPopupBaseView* view_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupBaseViewTest);
};

// Flaky on Win and Linux.  http://crbug.com/376299
#if defined(OS_LINUX) || defined(OS_WIN)
#define MAYBE_GestureTest DISABLED_GestureTest
#else
#define MAYBE_GestureTest GestureTest
#endif

IN_PROC_BROWSER_TEST_F(AutofillPopupBaseViewTest, MAYBE_GestureTest) {
  gfx::Rect bounds(0, 0, 5, 5);
  gfx::Point point = bounds.CenterPoint();
  EXPECT_CALL(mock_delegate_, popup_bounds()).WillRepeatedly(Return(bounds));

  ShowView();

  // Expectations.
  {
    testing::InSequence dummy;
    EXPECT_CALL(mock_delegate_, SetSelectionAtPoint(point)).Times(2);
    EXPECT_CALL(mock_delegate_, AcceptSelectedLine());
    EXPECT_CALL(mock_delegate_, SelectionCleared());
  }

  // Tap down will select an element.
  ui::GestureEvent tap_down_event = CreateGestureEvent(ui::ET_GESTURE_TAP_DOWN,
                                                       point);
  SimulateGesture(&tap_down_event);


  // Tapping will accept the selection.
  ui::GestureEvent tap_event = CreateGestureEvent(ui::ET_GESTURE_TAP, point);
  SimulateGesture(&tap_event);

  // Tapping outside the bounds clears any selection.
  ui::GestureEvent outside_tap = CreateGestureEvent(ui::ET_GESTURE_TAP,
                                                    gfx::Point(100, 100));
  SimulateGesture(&outside_tap);
}

IN_PROC_BROWSER_TEST_F(AutofillPopupBaseViewTest, DoubleClickTest) {
  gfx::Rect bounds(0, 0, 5, 5);
  EXPECT_CALL(mock_delegate_, popup_bounds()).WillRepeatedly(Return(bounds));

  ShowView();

  ui::MouseEvent mouse_down(ui::ET_MOUSE_PRESSED, gfx::Point(0, 0),
                            gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);
  EXPECT_TRUE(static_cast<views::View*>(view_)->OnMousePressed(mouse_down));

  // Ignore double clicks.
  mouse_down.SetClickCount(2);
  EXPECT_FALSE(static_cast<views::View*>(view_)->OnMousePressed(mouse_down));
}

// Regression test for crbug.com/391316
IN_PROC_BROWSER_TEST_F(AutofillPopupBaseViewTest, CorrectBoundsTest) {
  gfx::Rect bounds(100, 150, 5, 5);
  EXPECT_CALL(mock_delegate_, popup_bounds()).WillRepeatedly(Return(bounds));

  ShowView();

  gfx::Point display_point = static_cast<views::View*>(view_)
                                 ->GetWidget()
                                 ->GetClientAreaBoundsInScreen()
                                 .origin();
  gfx::Point expected_point = bounds.origin();
  EXPECT_EQ(expected_point, display_point);
}

IN_PROC_BROWSER_TEST_F(AutofillPopupBaseViewTest, MouseExitedTest) {
  for (bool has_selection : {true, false}) {
    EXPECT_CALL(mock_delegate_, HasSelection()).WillOnce(Return(has_selection));
    EXPECT_CALL(mock_delegate_, SelectionCleared())
        .Times(has_selection ? 1 : 0);

    ShowView();

    ui::MouseEvent exit_event(ui::ET_MOUSE_EXITED, gfx::Point(), gfx::Point(),
                              ui::EventTimeForNow(), 0, 0);
    static_cast<views::View*>(view_)->OnMouseExited(exit_event);

    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace autofill
