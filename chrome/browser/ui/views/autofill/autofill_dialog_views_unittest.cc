// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_dialog_views.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/autofill/mock_autofill_dialog_view_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/decorated_textfield.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "components/web_modal/test_web_contents_modal_dialog_host.h"
#include "components/web_modal/test_web_contents_modal_dialog_manager_delegate.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
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
  virtual ~TestAutofillDialogViews() {}

  using AutofillDialogViews::GetLoadingShieldForTesting;
  using AutofillDialogViews::GetSignInWebViewForTesting;
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

  virtual void TearDown() OVERRIDE {
    dialog_->GetWidget()->CloseNow();
    dialog_.reset();

    TestWithBrowserView::TearDown();
  }

  MockAutofillDialogViewDelegate* delegate() { return &view_delegate_; }

  TestAutofillDialogViews* dialog() { return dialog_.get(); }

 protected:
  void SetSectionsFocusable() {
    dialog()->GetLoadingShieldForTesting()->SetFocusable(true);
    // The sign in web view is not focusable until a web contents is created.
    // TODO(dbeam): figure out how to create a web contents on the right thread.
    dialog()->GetNotificationAreaForTesting()->SetFocusable(true);
    dialog()->GetScrollableAreaForTesting()->SetFocusable(true);
  }

 private:
  // Fake dialog delegate and host to isolate test behavior.
  web_modal::TestWebContentsModalDialogManagerDelegate dialog_delegate_;
  scoped_ptr<web_modal::TestWebContentsModalDialogHost> dialog_host_;

  // Mock view delegate as this file only tests the view.
  testing::NiceMock<MockAutofillDialogViewDelegate> view_delegate_;

  scoped_ptr<TestAutofillDialogViews> dialog_;
};

TEST_F(AutofillDialogViewsTest, InitialFocus) {
  views::FocusManager* focus_manager = dialog()->GetWidget()->GetFocusManager();
  views::View* focused_view = focus_manager->GetFocusedView();
  EXPECT_STREQ(DecoratedTextfield::kViewClassName,
               focused_view->GetClassName());
}

TEST_F(AutofillDialogViewsTest, SignInFocus) {
  SetSectionsFocusable();

  views::View* loading_shield = dialog()->GetLoadingShieldForTesting();
  views::View* sign_in_web_view = dialog()->GetSignInWebViewForTesting();
  views::View* notification_area = dialog()->GetNotificationAreaForTesting();
  views::View* scrollable_area = dialog()->GetScrollableAreaForTesting();

  dialog()->ShowSignIn();

  // The sign in view should be the only showing and focusable view.
  EXPECT_TRUE(sign_in_web_view->IsFocusable());
  EXPECT_FALSE(loading_shield->IsFocusable());
  EXPECT_FALSE(notification_area->IsFocusable());
  EXPECT_FALSE(scrollable_area->IsFocusable());

  EXPECT_CALL(*delegate(), ShouldShowSpinner()).WillRepeatedly(Return(false));
  dialog()->HideSignIn();

  // Hide sign in while not loading Wallet items as if the user clicked "Back".
  EXPECT_TRUE(notification_area->IsFocusable());
  EXPECT_TRUE(scrollable_area->IsFocusable());
  EXPECT_FALSE(loading_shield->IsFocusable());
  EXPECT_FALSE(sign_in_web_view->IsFocusable());

  dialog()->ShowSignIn();

  EXPECT_TRUE(sign_in_web_view->IsFocusable());
  EXPECT_FALSE(loading_shield->IsFocusable());
  EXPECT_FALSE(notification_area->IsFocusable());
  EXPECT_FALSE(scrollable_area->IsFocusable());

  EXPECT_CALL(*delegate(), ShouldShowSpinner()).WillRepeatedly(Return(true));
  dialog()->HideSignIn();

  // Hide sign in while pretending to load Wallet data.
  EXPECT_TRUE(loading_shield->IsFocusable());
  EXPECT_FALSE(notification_area->IsFocusable());
  EXPECT_FALSE(scrollable_area->IsFocusable());
  EXPECT_FALSE(sign_in_web_view->IsFocusable());
}

TEST_F(AutofillDialogViewsTest, LoadingFocus) {
  SetSectionsFocusable();

  views::View* loading_shield = dialog()->GetLoadingShieldForTesting();
  views::View* sign_in_web_view = dialog()->GetSignInWebViewForTesting();
  views::View* notification_area = dialog()->GetNotificationAreaForTesting();
  views::View* scrollable_area = dialog()->GetScrollableAreaForTesting();

  // Pretend as if loading Wallet data.
  EXPECT_CALL(*delegate(), ShouldShowSpinner()).WillRepeatedly(Return(true));
  dialog()->UpdateAccountChooser();

  EXPECT_TRUE(loading_shield->IsFocusable());
  EXPECT_FALSE(notification_area->IsFocusable());
  EXPECT_FALSE(scrollable_area->IsFocusable());
  EXPECT_FALSE(sign_in_web_view->IsFocusable());

  // Pretend as if Wallet data has finished loading.
  EXPECT_CALL(*delegate(), ShouldShowSpinner()).WillRepeatedly(Return(false));
  dialog()->UpdateAccountChooser();

  EXPECT_TRUE(notification_area->IsFocusable());
  EXPECT_TRUE(scrollable_area->IsFocusable());
  EXPECT_FALSE(loading_shield->IsFocusable());
  EXPECT_FALSE(sign_in_web_view->IsFocusable());
}

TEST_F(AutofillDialogViewsTest, ImeEventDoesntCrash) {
  // IMEs create synthetic events with no backing native event.
  views::FocusManager* focus_manager = dialog()->GetWidget()->GetFocusManager();
  views::View* focused_view = focus_manager->GetFocusedView();
  ASSERT_STREQ(DecoratedTextfield::kViewClassName,
               focused_view->GetClassName());
  EXPECT_FALSE(dialog()->HandleKeyEvent(
      static_cast<views::Textfield*>(focused_view),
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_A, 0, false)));
}

}  // namespace autofill
