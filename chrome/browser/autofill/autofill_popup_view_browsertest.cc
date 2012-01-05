// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_popup_view.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/url_constants.h"
#include "content/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestAutofillPopupView : public AutofillPopupView {
 public:
  explicit TestAutofillPopupView(content::WebContents* web_contents)
      : AutofillPopupView(web_contents) {}
  virtual ~TestAutofillPopupView() {}

  MOCK_METHOD0(Hide, void());

  virtual void Show(const std::vector<string16>& autofill_values,
                    const std::vector<string16>& autofill_labels,
                    const std::vector<string16>& autofill_icons,
                    const std::vector<int>& autofill_unique_ids,
                    int separator_index) OVERRIDE {}
};

class AutofillPopupViewBrowserTest : public InProcessBrowserTest {
 public:
  AutofillPopupViewBrowserTest() {}
  virtual ~AutofillPopupViewBrowserTest() {}

 protected:
  scoped_ptr<TestAutofillPopupView> autofill_popup_view_;
};


#if defined(OS_WIN)
// http://crbug.com/109269
#define MAYBE_SwitchTabAndHideAutofillPopup \
  DISABLED_SwitchTabAndHideAutofillPopup
#else
#define MAYBE_SwitchTabAndHideAutofillPopup SwitchTabAndHideAutofillPopup
#endif
IN_PROC_BROWSER_TEST_F(InProcessBrowserTest,
                       MAYBE_SwitchTabAndHideAutofillPopup) {
  content::WebContents* web_contents = browser()->GetSelectedWebContents();
  TestAutofillPopupView autofill_popup_view(web_contents);
  EXPECT_CALL(autofill_popup_view, Hide());

  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_WEB_CONTENTS_HIDDEN,
      content::Source<content::WebContents>(web_contents));
  browser()->AddSelectedTabWithURL(GURL(chrome::kAboutBlankURL),
                                   content::PAGE_TRANSITION_START_PAGE);
  observer.Wait();

  // The mock verifies that the call was made.
}

IN_PROC_BROWSER_TEST_F(InProcessBrowserTest,
                       TestPageNavigationHidingAutofillPopup) {
  content::WebContents* web_contents = browser()->GetSelectedWebContents();
  TestAutofillPopupView autofill_popup_view(web_contents);
  EXPECT_CALL(autofill_popup_view, Hide());

  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<content::NavigationController>(
          &(web_contents->GetController())));
  browser()->OpenURL(content::OpenURLParams(
      GURL(chrome::kAboutBlankURL), content::Referrer(),
      CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false));
  browser()->OpenURL(content::OpenURLParams(
      GURL(chrome::kAboutCrashURL), content::Referrer(),
      CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false));
  observer.Wait();

  // The mock verifies that the call was made.
}
