// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/omnibox/test_omnibox_client.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/views/test/views_test_base.h"

namespace {

class TestOmniboxPopupContentsView : public OmniboxPopupContentsView {
 public:
  explicit TestOmniboxPopupContentsView(OmniboxEditModel* edit_model)
      : OmniboxPopupContentsView(gfx::FontList(),
                                 nullptr,
                                 edit_model,
                                 nullptr) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestOmniboxPopupContentsView);
};

}  // namespace

class OmniboxResultViewTest : public views::ViewsTestBase {
 public:
  void SetUp() override {
    ViewsTestBase::SetUp();
    edit_model_ = base::MakeUnique<OmniboxEditModel>(
        nullptr, nullptr, base::MakeUnique<TestOmniboxClient>());
    popup_view_ =
        base::MakeUnique<TestOmniboxPopupContentsView>(edit_model_.get());
    result_view_ = base::MakeUnique<OmniboxResultView>(popup_view_.get(), 0,
                                                       gfx::FontList());
  }

  OmniboxResultView* result_view() { return result_view_.get(); }

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::unique_ptr<OmniboxEditModel> edit_model_;
  std::unique_ptr<TestOmniboxPopupContentsView> popup_view_;
  std::unique_ptr<OmniboxResultView> result_view_;
};

TEST_F(OmniboxResultViewTest, MouseMoveAndExit) {
  EXPECT_EQ(OmniboxResultView::NORMAL, result_view()->GetState());

  // Moving the mouse over the view should put it in the HOVERED state.
  ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0);
  result_view()->OnMouseMoved(event);
  EXPECT_EQ(OmniboxResultView::HOVERED, result_view()->GetState());

  // Continuing to move over the view should not change the state.
  result_view()->OnMouseMoved(event);
  EXPECT_EQ(OmniboxResultView::HOVERED, result_view()->GetState());

  // But exiting should revert the state to NORMAL.
  result_view()->OnMouseExited(event);
  EXPECT_EQ(OmniboxResultView::NORMAL, result_view()->GetState());
}

TEST_F(OmniboxResultViewTest, MousePressedCancelsHover) {
  ui::MouseEvent move_event(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                            ui::EventTimeForNow(), 0, 0);
  result_view()->OnMouseMoved(move_event);
  EXPECT_EQ(OmniboxResultView::HOVERED, result_view()->GetState());

  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             0);
  EXPECT_FALSE(result_view()->OnMousePressed(press_event));
  EXPECT_EQ(OmniboxResultView::NORMAL, result_view()->GetState());
}
