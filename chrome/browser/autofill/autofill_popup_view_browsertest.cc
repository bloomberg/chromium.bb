// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_popup_view.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autofill/test_autofill_external_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtLeast;
using testing::_;

namespace {

class MockAutofillExternalDelegate : public TestAutofillExternalDelegate {
 public:
  MockAutofillExternalDelegate() : TestAutofillExternalDelegate(NULL, NULL) {}
  ~MockAutofillExternalDelegate() {}

  virtual void SelectAutofillSuggestionAtIndex(int unique_id, int list_index)
      OVERRIDE {}
};

class TestAutofillPopupView : public AutofillPopupView {
 public:
  explicit TestAutofillPopupView(
      content::WebContents* web_contents,
      AutofillExternalDelegate* autofill_external_delegate)
      : AutofillPopupView(web_contents, autofill_external_delegate) {}
  virtual ~TestAutofillPopupView() {}

  MOCK_METHOD0(Hide, void());

  MOCK_METHOD1(InvalidateRow, void(size_t));

  void SetSelectedLine(size_t selected_line) {
    AutofillPopupView::SetSelectedLine(selected_line);
  }

 protected:
  virtual void ShowInternal() OVERRIDE {}

  virtual void HideInternal() OVERRIDE {}
};

}  // namespace

class AutofillPopupViewBrowserTest : public InProcessBrowserTest {
 public:
  AutofillPopupViewBrowserTest() {}
  virtual ~AutofillPopupViewBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    web_contents_ = browser()->GetSelectedWebContents();
    ASSERT_TRUE(web_contents_ != NULL);

    autofill_popup_view_.reset(new TestAutofillPopupView(
        web_contents_,
        &autofill_external_delegate_));
  }

 protected:
  content::WebContents* web_contents_;
  scoped_ptr<TestAutofillPopupView> autofill_popup_view_;
  MockAutofillExternalDelegate autofill_external_delegate_;
};

IN_PROC_BROWSER_TEST_F(AutofillPopupViewBrowserTest,
                       SwitchTabAndHideAutofillPopup) {
  EXPECT_CALL(*autofill_popup_view_, Hide()).Times(AtLeast(1));

  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_WEB_CONTENTS_HIDDEN,
      content::Source<content::WebContents>(web_contents_));
  browser()->AddSelectedTabWithURL(GURL(chrome::kAboutBlankURL),
                                   content::PAGE_TRANSITION_START_PAGE);
  observer.Wait();

  // The mock verifies that the call was made.
}

IN_PROC_BROWSER_TEST_F(AutofillPopupViewBrowserTest,
                       TestPageNavigationHidingAutofillPopup) {
  EXPECT_CALL(*autofill_popup_view_, Hide()).Times(AtLeast(1));

  ui_test_utils::WindowedNotificationObserver observer(
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

IN_PROC_BROWSER_TEST_F(AutofillPopupViewBrowserTest,
                       SetSelectedAutofillLineAndCallInvalidate) {
  std::vector<string16> autofill_values;
  autofill_values.push_back(string16());
  std::vector<int> autofill_ids;
  autofill_ids.push_back(0);
  autofill_popup_view_->Show(
      autofill_values, autofill_values, autofill_values, autofill_ids, 0);

  // Make sure that when a new line is selected, it is invalidated so it can
  // be updated to show it is selected.
  int selected_line = 0;
  EXPECT_CALL(*autofill_popup_view_, InvalidateRow(selected_line));
  autofill_popup_view_->SetSelectedLine(selected_line);

  // Ensure that the row isn't invalidated if it didn't change.
  EXPECT_CALL(*autofill_popup_view_, InvalidateRow(selected_line)).Times(0);
  autofill_popup_view_->SetSelectedLine(selected_line);

  // Change back to no selection.
  EXPECT_CALL(*autofill_popup_view_, InvalidateRow(selected_line));
  autofill_popup_view_->SetSelectedLine(-1);
}
