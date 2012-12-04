// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_external_delegate_views.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/test_autofill_external_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/autofill/autofill_popup_view_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace {

class MockAutofillExternalDelegateViews : public AutofillExternalDelegateViews {
 public:
  explicit MockAutofillExternalDelegateViews(content::WebContents* web_contents)
      : AutofillExternalDelegateViews(
            web_contents,
            AutofillManager::FromWebContents(web_contents)),
        popup_hidden_(false) {}
  ~MockAutofillExternalDelegateViews() {}

  void HideAutofillPopupInternal() OVERRIDE {
    popup_hidden_ = true;
    AutofillExternalDelegateViews::HideAutofillPopupInternal();
  }

  AutofillPopupViewViews* popup_view() {
    return AutofillExternalDelegateViews::popup_view();
  }

  virtual void ClearPreviewedForm() OVERRIDE {}

  bool popup_hidden_;
};

}  // namespace

class AutofillExternalDelegateViewsBrowserTest : public InProcessBrowserTest {
 public:
  AutofillExternalDelegateViewsBrowserTest() {}
  virtual ~AutofillExternalDelegateViewsBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    web_contents_ = chrome::GetActiveWebContents(browser());
    ASSERT_TRUE(web_contents_ != NULL);

    autofill_external_delegate_.reset(
        new MockAutofillExternalDelegateViews(web_contents_));
  }

 protected:
  content::WebContents* web_contents_;
  scoped_ptr<MockAutofillExternalDelegateViews> autofill_external_delegate_;
};

IN_PROC_BROWSER_TEST_F(AutofillExternalDelegateViewsBrowserTest,
                       OpenAndClosePopup) {
  autofill::GenerateTestAutofillPopup(autofill_external_delegate_.get());

  autofill_external_delegate_->HideAutofillPopup();
  EXPECT_TRUE(autofill_external_delegate_->popup_hidden_);
}

// See http://crbug.com/164019
#if !defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(AutofillExternalDelegateViewsBrowserTest,
                       CloseWidgetAndNoLeaking) {
  autofill::GenerateTestAutofillPopup(autofill_external_delegate_.get());
  // Delete the widget to ensure that the external delegate can handle the
  // popup getting deleted elsewhere.
  views::Widget* popup_widget =
      autofill_external_delegate_->popup_view()->GetWidget();
  popup_widget->CloseNow();

  EXPECT_TRUE(autofill_external_delegate_->popup_hidden_);
}
#endif  // !defined(USE_AURA)

IN_PROC_BROWSER_TEST_F(AutofillExternalDelegateViewsBrowserTest,
                       HandlePopupClosingAndChangingPages) {
  autofill::GenerateTestAutofillPopup(autofill_external_delegate_.get());

  // Close popup.
  autofill_external_delegate_->HideAutofillPopup();

  // Navigate to a new page
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<content::NavigationController>(
          &(web_contents_->GetController())));
  browser()->OpenURL(content::OpenURLParams(
      GURL(chrome::kAboutBlankURL), content::Referrer(),
      CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false));
  observer.Wait();
}
