// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_dialog_views.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/autofill/mock_autofill_dialog_view_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "components/web_modal/test_web_contents_modal_dialog_host.h"
#include "components/web_modal/test_web_contents_modal_dialog_manager_delegate.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

using web_modal::WebContentsModalDialogManager;

// A views implementation of the Autofill dialog with slightly more testability.
class TestAutofillDialogViews : public AutofillDialogViews {
 public:
  explicit TestAutofillDialogViews(AutofillDialogViewDelegate* delegate)
      : AutofillDialogViews(delegate) {}
  virtual ~TestAutofillDialogViews() {}

  void ShowLoadingMode() { ShowDialogInMode(LOADING); }
  void ShowSignInMode() { ShowDialogInMode(SIGN_IN); }
  void ShowDetailInputMode() { ShowDialogInMode(DETAIL_INPUT); }

  using AutofillDialogViews::GetLoadingShieldForTesting;
  using AutofillDialogViews::GetSignInWebviewForTesting;
  using AutofillDialogViews::GetNotificationAreaForTesting;
  using AutofillDialogViews::GetScrollableAreaForTesting;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogViews);
};

}  // namespace

class AutofillDialogViewsTest : public TestWithBrowserView {
 public:
  AutofillDialogViewsTest() {}
  virtual ~AutofillDialogViewsTest() {}

  // TestWithBrowserView:
  virtual void SetUp() OVERRIDE {
    TestWithBrowserView::SetUp();

    AddTab(browser(), GURL());
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

  virtual void TearDown() OVERRIDE {
    dialog_->GetWidget()->CloseNow();

    TestWithBrowserView::TearDown();
  }

  TestAutofillDialogViews* dialog() { return dialog_.get(); }

 private:
  // Fake dialog delegate and host to isolate test behavior.
  web_modal::TestWebContentsModalDialogManagerDelegate dialog_delegate_;
  scoped_ptr<web_modal::TestWebContentsModalDialogHost> dialog_host_;

  // Mock view delegate as this file only tests the view.
  testing::NiceMock<MockAutofillDialogViewDelegate> view_delegate_;

  scoped_ptr<TestAutofillDialogViews> dialog_;
};

// Switching between dialog modes should make visually hidden views !visible().
TEST_F(AutofillDialogViewsTest, ShowDialogInMode) {
  dialog()->ShowLoadingMode();
  EXPECT_TRUE(dialog()->GetLoadingShieldForTesting()->visible());
  EXPECT_FALSE(dialog()->GetSignInWebviewForTesting()->visible());
  EXPECT_FALSE(dialog()->GetNotificationAreaForTesting()->visible());
  EXPECT_FALSE(dialog()->GetScrollableAreaForTesting()->visible());

  dialog()->ShowSignInMode();
  EXPECT_FALSE(dialog()->GetLoadingShieldForTesting()->visible());
  EXPECT_TRUE(dialog()->GetSignInWebviewForTesting()->visible());
  EXPECT_FALSE(dialog()->GetNotificationAreaForTesting()->visible());
  EXPECT_FALSE(dialog()->GetScrollableAreaForTesting()->visible());

  dialog()->ShowDetailInputMode();
  EXPECT_FALSE(dialog()->GetLoadingShieldForTesting()->visible());
  EXPECT_FALSE(dialog()->GetSignInWebviewForTesting()->visible());
  EXPECT_TRUE(dialog()->GetNotificationAreaForTesting()->visible());
  EXPECT_TRUE(dialog()->GetScrollableAreaForTesting()->visible());
}

}  // namespace autofill
