// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"

#include "base/metrics/histogram_samples.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kDisplayDispositionMetric[] = "PasswordBubble.DisplayDisposition";

}  // namespace

namespace metrics_util = password_manager::metrics_util;

class ManagePasswordsBubbleViewTest : public ManagePasswordsTest {
 public:
  ManagePasswordsBubbleViewTest() {}
  virtual ~ManagePasswordsBubbleViewTest() {}

  virtual ManagePasswordsIcon* view() OVERRIDE {
    BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
    return browser_view->GetToolbarView()
        ->location_bar()
        ->manage_passwords_icon_view();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleViewTest);
};

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleViewTest, BasicOpenAndClose) {
  EXPECT_FALSE(ManagePasswordsBubbleView::IsShowing());
  ManagePasswordsBubbleView::ShowBubble(
      browser()->tab_strip_model()->GetActiveWebContents(),
      ManagePasswordsBubble::USER_ACTION);
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());
  const ManagePasswordsBubbleView* bubble =
      ManagePasswordsBubbleView::manage_password_bubble();
  EXPECT_TRUE(bubble->initially_focused_view());
  EXPECT_EQ(bubble->initially_focused_view(),
            bubble->GetFocusManager()->GetFocusedView());
  ManagePasswordsBubbleView::CloseBubble();
  EXPECT_FALSE(ManagePasswordsBubbleView::IsShowing());

  // And, just for grins, ensure that we can re-open the bubble.
  ManagePasswordsBubbleView::ShowBubble(
      browser()->tab_strip_model()->GetActiveWebContents(),
      ManagePasswordsBubble::USER_ACTION);
  EXPECT_TRUE(ManagePasswordsBubbleView::manage_password_bubble()->
      GetFocusManager()->GetFocusedView());
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());
  ManagePasswordsBubbleView::CloseBubble();
  EXPECT_FALSE(ManagePasswordsBubbleView::IsShowing());
}

// Same as 'BasicOpenAndClose', but use the command rather than the static
// method directly.
IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleViewTest, CommandControlsBubble) {
  // The command only works if the icon is visible, so get into management mode.
  SetupManagingPasswords();
  EXPECT_FALSE(ManagePasswordsBubbleView::IsShowing());
  ExecuteManagePasswordsCommand();
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());
  const ManagePasswordsBubbleView* bubble =
      ManagePasswordsBubbleView::manage_password_bubble();
  EXPECT_TRUE(bubble->initially_focused_view());
  EXPECT_EQ(bubble->initially_focused_view(),
            bubble->GetFocusManager()->GetFocusedView());
  ManagePasswordsBubbleView::CloseBubble();
  EXPECT_FALSE(ManagePasswordsBubbleView::IsShowing());

  // And, just for grins, ensure that we can re-open the bubble.
  ExecuteManagePasswordsCommand();
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());
  ManagePasswordsBubbleView::CloseBubble();
  EXPECT_FALSE(ManagePasswordsBubbleView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleViewTest,
                       CommandExecutionInManagingState) {
  SetupManagingPasswords();
  ExecuteManagePasswordsCommand();

  scoped_ptr<base::HistogramSamples> samples(
      GetSamples(kDisplayDispositionMetric));
  EXPECT_EQ(
      0,
      samples->GetCount(
          metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING));
  EXPECT_EQ(0,
            samples->GetCount(
                metrics_util::MANUAL_WITH_PASSWORD_PENDING));
  EXPECT_EQ(1,
            samples->GetCount(
                metrics_util::MANUAL_MANAGE_PASSWORDS));
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleViewTest,
                       CommandExecutionInAutomaticState) {
  // Open with pending password: automagical!
  SetupPendingPassword();

  // Bubble should not be focused by default.
  EXPECT_FALSE(ManagePasswordsBubbleView::manage_password_bubble()->
      GetFocusManager()->GetFocusedView());
  // Bubble can be active if user clicks it.
  EXPECT_TRUE(ManagePasswordsBubbleView::manage_password_bubble()->
      CanActivate());

  scoped_ptr<base::HistogramSamples> samples(
      GetSamples(kDisplayDispositionMetric));
  EXPECT_EQ(
      1,
      samples->GetCount(
          metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING));
  EXPECT_EQ(0,
            samples->GetCount(
                metrics_util::MANUAL_WITH_PASSWORD_PENDING));
  EXPECT_EQ(0,
            samples->GetCount(
                metrics_util::MANUAL_MANAGE_PASSWORDS));
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleViewTest,
                       CommandExecutionInPendingState) {
  // Open once with pending password: automagical!
  SetupPendingPassword();
  ManagePasswordsBubbleView::CloseBubble();
  // This opening should be measured as manual.
  ExecuteManagePasswordsCommand();

  scoped_ptr<base::HistogramSamples> samples(
      GetSamples(kDisplayDispositionMetric));
  EXPECT_EQ(
      1,
      samples->GetCount(
          metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING));
  EXPECT_EQ(1,
            samples->GetCount(
                metrics_util::MANUAL_WITH_PASSWORD_PENDING));
  EXPECT_EQ(0,
            samples->GetCount(
                metrics_util::MANUAL_MANAGE_PASSWORDS));
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleViewTest,
                       CommandExecutionInAutomaticSaveState) {
  SetupAutomaticPassword();
  ManagePasswordsBubbleView::CloseBubble();
  // Re-opening should count as manual.
  ExecuteManagePasswordsCommand();

 scoped_ptr<base::HistogramSamples> samples(
      GetSamples(kDisplayDispositionMetric));
  EXPECT_EQ(
      1,
      samples->GetCount(
          metrics_util::AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION));
  EXPECT_EQ(0,
            samples->GetCount(
                metrics_util::MANUAL_WITH_PASSWORD_PENDING));
  EXPECT_EQ(1,
            samples->GetCount(
                metrics_util::MANUAL_MANAGE_PASSWORDS));
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleViewTest, CloseOnClick) {
  ManagePasswordsBubbleView::ShowBubble(
      browser()->tab_strip_model()->GetActiveWebContents(),
      ManagePasswordsBubble::AUTOMATIC);
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());
  EXPECT_FALSE(ManagePasswordsBubbleView::manage_password_bubble()->
      GetFocusManager()->GetFocusedView());
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_FALSE(ManagePasswordsBubbleView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleViewTest, CloseOnKey) {
  content::WindowedNotificationObserver focus_observer(
      content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html;charset=utf-8,<input type=\"text\" autofocus>"));
  focus_observer.Wait();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ManagePasswordsBubbleView::ShowBubble(web_contents,
                                        ManagePasswordsBubble::AUTOMATIC);
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());
  EXPECT_FALSE(ManagePasswordsBubbleView::manage_password_bubble()->
      GetFocusManager()->GetFocusedView());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_TRUE(web_contents->GetRenderViewHost()->IsFocusedElementEditable());
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_K,
      false, false, false, false));
  EXPECT_FALSE(ManagePasswordsBubbleView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleViewTest, TwoTabsWithBubble) {
  // Set up the first tab with the bubble.
  SetupPendingPassword();
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());
  // Set up the second tab.
  AddTabAtIndex(0, GURL("chrome://newtab"), ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(ManagePasswordsBubbleView::IsShowing());
  ManagePasswordsBubbleView::ShowBubble(
      browser()->tab_strip_model()->GetActiveWebContents(),
      ManagePasswordsBubble::AUTOMATIC);
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());
  TabStripModel* tab_model = browser()->tab_strip_model();
  EXPECT_EQ(0, tab_model->active_index());
  // Back to the first tab.
  tab_model->ActivateTabAt(1, true);
  EXPECT_FALSE(ManagePasswordsBubbleView::IsShowing());
}
