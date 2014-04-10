// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"

#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
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
  // TODO(jdduke): Mock this method upon resolution of crbug.com/352463.
  MOCK_CONST_METHOD0(popup_bounds, gfx::Rect&());
  MOCK_METHOD0(container_view, gfx::NativeView());
};

}  // namespace

class AutofillPopupBaseViewTest : public InProcessBrowserTest {
 public:
  AutofillPopupBaseViewTest() {}
  virtual ~AutofillPopupBaseViewTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    gfx::NativeWindow window = browser()->window()->GetNativeWindow();
    EXPECT_CALL(mock_delegate_, container_view())
        .WillRepeatedly(Return(window));
    EXPECT_CALL(mock_delegate_, ViewDestroyed());

    view_ = new AutofillPopupBaseView(
        &mock_delegate_,
        views::Widget::GetWidgetForNativeWindow(window));
  }

  void ShowView() {
    view_->DoShow();
  }

  ui::GestureEvent CreateGestureEvent(ui::EventType type, gfx::Point point) {
    return ui::GestureEvent(type,
                            point.x(),
                            point.y(),
                            0,
                            ui::EventTimeForNow(),
                            ui::GestureEventDetails(type, 0, 0),
                            0);
  }

  void SimulateGesture(ui::GestureEvent* event) {
    view_->OnGestureEvent(event);
  }

 protected:
  MockAutofillPopupViewDelegate mock_delegate_;
  AutofillPopupBaseView* view_;
};

IN_PROC_BROWSER_TEST_F(AutofillPopupBaseViewTest, GestureTest) {
  gfx::Rect bounds(0, 0, 5, 5);
  gfx::Point point = bounds.CenterPoint();
  EXPECT_CALL(mock_delegate_, popup_bounds()).WillRepeatedly(ReturnRef(bounds));

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

}  // namespace autofill
