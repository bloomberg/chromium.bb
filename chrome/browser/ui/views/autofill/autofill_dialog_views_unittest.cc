// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_dialog_views.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/autofill/mock_autofill_dialog_view_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/decorated_textfield.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "components/web_modal/test_web_contents_modal_dialog_host.h"
#include "components/web_modal/test_web_contents_modal_dialog_manager_delegate.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/url_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

using testing::Return;
using web_modal::WebContentsModalDialogManager;

// A views implementation of the Autofill dialog with slightly more testability.
class TestAutofillDialogViews : public AutofillDialogViews {
 public:
  explicit TestAutofillDialogViews(AutofillDialogViewDelegate* delegate)
      : AutofillDialogViews(delegate) {}
  ~TestAutofillDialogViews() override {}

  using AutofillDialogViews::GetNotificationAreaForTesting;
  using AutofillDialogViews::GetScrollableAreaForTesting;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogViews);
};

}  // namespace

class AutofillDialogViewsTest : public TestWithBrowserView {
 public:
  AutofillDialogViewsTest() {}
  ~AutofillDialogViewsTest() override {}

  // TestWithBrowserView:
  void SetUp() override {
    TestWithBrowserView::SetUp();

    view_delegate_.SetProfile(profile());

    AddTab(browser(), GURL(url::kAboutBlankURL));
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(0);
    ASSERT_TRUE(contents);
    view_delegate_.SetWebContents(contents);

    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    dialog_host_.reset(new web_modal::TestWebContentsModalDialogHost(
        browser_view->GetWidget()->GetNativeView()));
    dialog_delegate_.set_web_contents_modal_dialog_host(dialog_host_.get());

    WebContentsModalDialogManager* dialog_manager =
        WebContentsModalDialogManager::FromWebContents(contents);
    ASSERT_TRUE(dialog_manager);
    dialog_manager->SetDelegate(&dialog_delegate_);

    dialog_.reset(new TestAutofillDialogViews(&view_delegate_));
    dialog_->Show();
  }

  void TearDown() override {
    dialog_->GetWidget()->CloseNow();
    dialog_.reset();

    TestWithBrowserView::TearDown();
  }

  MockAutofillDialogViewDelegate* delegate() { return &view_delegate_; }

  TestAutofillDialogViews* dialog() { return dialog_.get(); }

 protected:
  void SetSectionsFocusable() {
    dialog()->GetNotificationAreaForTesting()->SetFocusBehavior(
        views::View::FocusBehavior::ALWAYS);
    dialog()->GetScrollableAreaForTesting()->SetFocusBehavior(
        views::View::FocusBehavior::ALWAYS);
  }

 private:
  // Fake dialog delegate and host to isolate test behavior.
  web_modal::TestWebContentsModalDialogManagerDelegate dialog_delegate_;
  std::unique_ptr<web_modal::TestWebContentsModalDialogHost> dialog_host_;

  // Mock view delegate as this file only tests the view.
  testing::NiceMock<MockAutofillDialogViewDelegate> view_delegate_;

  std::unique_ptr<TestAutofillDialogViews> dialog_;
};

TEST_F(AutofillDialogViewsTest, InitialFocus) {
  views::FocusManager* focus_manager = dialog()->GetWidget()->GetFocusManager();
  views::View* focused_view = focus_manager->GetFocusedView();
  EXPECT_STREQ(DecoratedTextfield::kViewClassName,
               focused_view->GetClassName());
}

TEST_F(AutofillDialogViewsTest, ImeEventDoesntCrash) {
  // IMEs create synthetic events with no backing native event.
  views::FocusManager* focus_manager = dialog()->GetWidget()->GetFocusManager();
  views::View* focused_view = focus_manager->GetFocusedView();
  ASSERT_STREQ(DecoratedTextfield::kViewClassName,
               focused_view->GetClassName());
  EXPECT_FALSE(dialog()->HandleKeyEvent(
      static_cast<views::Textfield*>(focused_view),
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE)));
}

}  // namespace autofill
