// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/test_autofill_external_delegate.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"

using ::testing::AtLeast;
using testing::_;

namespace {

class MockAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  explicit MockAutofillExternalDelegate(content::WebContents* web_contents)
      : AutofillExternalDelegate(
            web_contents,
            AutofillManager::FromWebContents(web_contents)) {}
  ~MockAutofillExternalDelegate() {}

  virtual void DidSelectSuggestion(int unique_id) OVERRIDE {}

  virtual void ClearPreviewedForm() OVERRIDE {}

  AutofillPopupControllerImpl* GetController() {
    return controller();
  }

  MOCK_METHOD0(HideAutofillPopup, void());
};

}  // namespace

class AutofillExternalDelegateBrowserTest
    : public InProcessBrowserTest,
      public content::WebContentsObserver {
 public:
  AutofillExternalDelegateBrowserTest() {}
  virtual ~AutofillExternalDelegateBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    web_contents_ = chrome::GetActiveWebContents(browser());
    ASSERT_TRUE(web_contents_ != NULL);
    Observe(web_contents_);

    autofill_external_delegate_.reset(
        new MockAutofillExternalDelegate(web_contents_));
  }

  // Normally the WebContents will automatically delete the delegate, but here
  // the delegate is owned by this test, so we have to manually destroy.
  virtual void WebContentsDestroyed(content::WebContents* web_contents)
      OVERRIDE {
    DCHECK_EQ(web_contents_, web_contents);
    autofill_external_delegate_.reset();
  }

 protected:
  content::WebContents* web_contents_;
  scoped_ptr<MockAutofillExternalDelegate> autofill_external_delegate_;
};

IN_PROC_BROWSER_TEST_F(AutofillExternalDelegateBrowserTest,
                       SwitchTabAndHideAutofillPopup) {
  EXPECT_CALL(*autofill_external_delegate_,
              HideAutofillPopup()).Times(AtLeast(1));

  autofill::GenerateTestAutofillPopup(autofill_external_delegate_.get());

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED,
      content::Source<content::WebContents>(web_contents_));
  chrome::AddSelectedTabWithURL(browser(), GURL(chrome::kAboutBlankURL),
                                content::PAGE_TRANSITION_AUTO_TOPLEVEL);
  observer.Wait();

  // The mock verifies that the call was made.
}

IN_PROC_BROWSER_TEST_F(AutofillExternalDelegateBrowserTest,
                       TestPageNavigationHidingAutofillPopup) {
  EXPECT_CALL(*autofill_external_delegate_,
              HideAutofillPopup()).Times(AtLeast(1));

  autofill::GenerateTestAutofillPopup(autofill_external_delegate_.get());

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<content::NavigationController>(
          &(web_contents_->GetController())));
  browser()->OpenURL(content::OpenURLParams(
      GURL(chrome::kAboutBlankURL), content::Referrer(),
      CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false));
  browser()->OpenURL(content::OpenURLParams(
      GURL(chrome::kChromeUIAboutURL), content::Referrer(),
      CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false));
  observer.Wait();

  // The mock verifies that the call was made.
}

// Tests that closing the widget does not leak any resources.  This test is
// only really meaningful when run on the memory bots.
IN_PROC_BROWSER_TEST_F(AutofillExternalDelegateBrowserTest,
                       CloseWidgetAndNoLeaking) {
  autofill::GenerateTestAutofillPopup(autofill_external_delegate_.get());

  // Delete the view from under the delegate to ensure that the
  // delegate and the controller can handle the popup getting deleted elsewhere.
  autofill_external_delegate_->GetController()->view()->Hide();
}
