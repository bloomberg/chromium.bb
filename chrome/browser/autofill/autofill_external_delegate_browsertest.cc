// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/test_autofill_external_delegate.h"
#include "chrome/browser/autofill/test_autofill_manager_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
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

namespace {

class MockAutofillManagerDelegate
    : public autofill::TestAutofillManagerDelegate {
 public:
  virtual PrefService* GetPrefs() { return &prefs_; }

  PrefRegistrySyncable* GetPrefRegistry() {
    return prefs_.registry();
  }

  MOCK_METHOD6(ShowAutofillPopup,
               void(const gfx::RectF& element_bounds,
                    const std::vector<string16>& values,
                    const std::vector<string16>& labels,
                    const std::vector<string16>& icons,
                    const std::vector<int>& identifiers,
                    AutofillPopupDelegate* delegate));

  MOCK_METHOD0(HideAutofillPopup, void());

 private:
  TestingPrefServiceSyncable prefs_;
};

// Subclass AutofillManager so we can create AutofillManager instance.
class TestAutofillManager : public AutofillManager {
 public:
  TestAutofillManager(content::WebContents* web_contents,
                      autofill::AutofillManagerDelegate* delegate)
      : AutofillManager(web_contents, delegate) {}
  virtual ~TestAutofillManager() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAutofillManager);
};

class TestAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  TestAutofillExternalDelegate(content::WebContents* web_contents,
                               AutofillManager* autofill_manager)
      : AutofillExternalDelegate(web_contents, autofill_manager) {}
  ~TestAutofillExternalDelegate() {}
};

}  // namespace

class AutofillExternalDelegateBrowserTest
    : public InProcessBrowserTest,
      public content::WebContentsObserver {
 public:
  AutofillExternalDelegateBrowserTest() {}
  virtual ~AutofillExternalDelegateBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents_ != NULL);
    Observe(web_contents_);

    AutofillManager::RegisterUserPrefs(manager_delegate_.GetPrefRegistry());

    autofill_manager_.reset(
        new TestAutofillManager(web_contents_, &manager_delegate_));
    autofill_external_delegate_.reset(
        new TestAutofillExternalDelegate(web_contents_,
                                         autofill_manager_.get()));
  }

  // Normally the WebContents will automatically delete the delegate, but here
  // the delegate is owned by this test, so we have to manually destroy.
  virtual void WebContentsDestroyed(content::WebContents* web_contents)
      OVERRIDE {
    DCHECK_EQ(web_contents_, web_contents);
    autofill_external_delegate_.reset();
    autofill_manager_.reset();
  }

 protected:
  content::WebContents* web_contents_;

  testing::NiceMock<MockAutofillManagerDelegate> manager_delegate_;
  scoped_ptr<TestAutofillManager> autofill_manager_;
  scoped_ptr<TestAutofillExternalDelegate> autofill_external_delegate_;
};

IN_PROC_BROWSER_TEST_F(AutofillExternalDelegateBrowserTest,
                       SwitchTabAndHideAutofillPopup) {
  autofill::GenerateTestAutofillPopup(autofill_external_delegate_.get());

  // Notification is different on platforms. On linux this will be called twice,
  // while on windows only once.
  EXPECT_CALL(manager_delegate_, HideAutofillPopup())
      .Times(testing::AtLeast(1));

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED,
      content::Source<content::WebContents>(web_contents_));
  chrome::AddSelectedTabWithURL(browser(), GURL(chrome::kAboutBlankURL),
                                content::PAGE_TRANSITION_AUTO_TOPLEVEL);
  observer.Wait();
}

IN_PROC_BROWSER_TEST_F(AutofillExternalDelegateBrowserTest,
                       TestPageNavigationHidingAutofillPopup) {
  autofill::GenerateTestAutofillPopup(autofill_external_delegate_.get());

  // Notification is different on platforms. On linux this will be called twice,
  // while on windows only once.
  EXPECT_CALL(manager_delegate_, HideAutofillPopup())
      .Times(testing::AtLeast(1));

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<content::NavigationController>(
          &(web_contents_->GetController())));
  browser()->OpenURL(content::OpenURLParams(
      GURL(chrome::kChromeUIBookmarksURL), content::Referrer(),
      CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false));
  browser()->OpenURL(content::OpenURLParams(
      GURL(chrome::kChromeUIAboutURL), content::Referrer(),
      CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false));
  observer.Wait();
}
