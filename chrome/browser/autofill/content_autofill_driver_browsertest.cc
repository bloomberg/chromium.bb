// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace autofill {
namespace {

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() {}
  virtual ~MockAutofillClient() {}

  virtual PrefService* GetPrefs() { return &prefs_; }

  user_prefs::PrefRegistrySyncable* GetPrefRegistry() {
    return prefs_.registry();
  }

  MOCK_METHOD4(ShowAutofillPopup,
               void(const gfx::RectF& element_bounds,
                    base::i18n::TextDirection text_direction,
                    const std::vector<autofill::Suggestion>& suggestions,
                    base::WeakPtr<AutofillPopupDelegate> delegate));

  MOCK_METHOD0(HideAutofillPopup, void());

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;

  DISALLOW_COPY_AND_ASSIGN(MockAutofillClient);
};

// Subclass ContentAutofillDriver so we can create an ContentAutofillDriver
// instance.
class TestContentAutofillDriver : public ContentAutofillDriver {
 public:
  TestContentAutofillDriver(content::RenderFrameHost* rfh,
                            AutofillClient* client)
      : ContentAutofillDriver(
            rfh,
            client,
            g_browser_process->GetApplicationLocale(),
            AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER) {}
  ~TestContentAutofillDriver() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestContentAutofillDriver);
};

}  // namespace

class ContentAutofillDriverBrowserTest : public InProcessBrowserTest,
                                         public content::WebContentsObserver {
 public:
  ContentAutofillDriverBrowserTest() {}
  virtual ~ContentAutofillDriverBrowserTest() {}

  void SetUpOnMainThread() override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents != NULL);
    Observe(web_contents);
    AutofillManager::RegisterProfilePrefs(autofill_client_.GetPrefRegistry());

    web_contents->RemoveUserData(
        ContentAutofillDriverFactory::
            kContentAutofillDriverFactoryWebContentsUserDataKey);
    ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
        web_contents, &autofill_client_, "en-US",
        AutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER);
  }

  void WasHidden() override {
    if (!web_contents_hidden_callback_.is_null())
      web_contents_hidden_callback_.Run();
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->HasCommitted())
      return;

    if (!nav_entry_committed_callback_.is_null())
      nav_entry_committed_callback_.Run();
  }

 protected:
  base::Closure web_contents_hidden_callback_;
  base::Closure nav_entry_committed_callback_;

  testing::NiceMock<MockAutofillClient> autofill_client_;
};

IN_PROC_BROWSER_TEST_F(ContentAutofillDriverBrowserTest,
                       SwitchTabAndHideAutofillPopup) {
  // Notification is different on platforms. On linux this will be called twice,
  // while on windows only once.
  EXPECT_CALL(autofill_client_, HideAutofillPopup()).Times(testing::AtLeast(1));

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  web_contents_hidden_callback_ = runner->QuitClosure();
  chrome::AddSelectedTabWithURL(browser(),
                                GURL(url::kAboutBlankURL),
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  runner->Run();
  web_contents_hidden_callback_.Reset();
}

IN_PROC_BROWSER_TEST_F(ContentAutofillDriverBrowserTest,
                       TestPageNavigationHidingAutofillPopup) {
  // Notification is different on platforms. On linux this will be called twice,
  // while on windows only once.
  EXPECT_CALL(autofill_client_, HideAutofillPopup()).Times(testing::AtLeast(1));

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  nav_entry_committed_callback_ = runner->QuitClosure();
  browser()->OpenURL(content::OpenURLParams(
      GURL(chrome::kChromeUIBookmarksURL), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
  browser()->OpenURL(content::OpenURLParams(
      GURL(chrome::kChromeUIAboutURL), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
  runner->Run();
  nav_entry_committed_callback_.Reset();
}

}  // namespace autofill
