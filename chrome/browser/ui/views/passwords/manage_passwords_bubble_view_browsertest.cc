// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"

#include "base/command_line.h"
#include "base/metrics/histogram_samples.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
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
#include "content/public/common/content_switches.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::Field;
using testing::_;

namespace {

const char kDisplayDispositionMetric[] = "PasswordBubble.DisplayDisposition";

// A helper class that will create FakeURLFetcher and record the requested URLs.
class TestURLFetcherCallback {
 public:
  scoped_ptr<net::FakeURLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcherDelegate* d,
      const std::string& response_data,
      net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status) {
    OnRequestDone(url);
    return scoped_ptr<net::FakeURLFetcher>(new net::FakeURLFetcher(
        url, d, response_data, response_code, status));
  }

  MOCK_METHOD1(OnRequestDone, void(const GURL&));
};

}  // namespace

namespace metrics_util = password_manager::metrics_util;

class ManagePasswordsBubbleViewTest : public ManagePasswordsTest {
 public:
  ManagePasswordsBubbleViewTest() {}
  ~ManagePasswordsBubbleViewTest() override {}

  ManagePasswordsIcon* view() override {
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
  EXPECT_FALSE(ManagePasswordsBubbleView::IsShowing());
  ExecuteManagePasswordsCommand();
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());

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
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());

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
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());
  ManagePasswordsBubbleView::CloseBubble();
  // This opening should be measured as manual.
  ExecuteManagePasswordsCommand();
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());

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
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());
  ManagePasswordsBubbleView::CloseBubble();
  // Re-opening should count as manual.
  ExecuteManagePasswordsCommand();
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());

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

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleViewTest, ChooseCredential) {
  GURL origin("https://example.com");
  ScopedVector<autofill::PasswordForm> local_credentials;
  test_form()->origin = origin;
  test_form()->display_name = base::ASCIIToUTF16("Peter");
  test_form()->username_value = base::ASCIIToUTF16("pet12@gmail.com");
  test_form()->avatar_url = GURL("broken url");
  local_credentials.push_back(new autofill::PasswordForm(*test_form()));
  ScopedVector<autofill::PasswordForm> federated_credentials;
  GURL avatar_url("https://google.com/avatar.png");
  test_form()->avatar_url = avatar_url;
  test_form()->display_name = base::ASCIIToUTF16("Peter Pen");
  test_form()->federation_url = GURL("https://google.com/federation");
  federated_credentials.push_back(new autofill::PasswordForm(*test_form()));

  // Prepare to capture the network request.
  TestURLFetcherCallback url_callback;
  net::FakeURLFetcherFactory factory(
      NULL,
      base::Bind(&TestURLFetcherCallback::CreateURLFetcher,
                 base::Unretained(&url_callback)));
  factory.SetFakeResponse(avatar_url, std::string(), net::HTTP_OK,
                          net::URLRequestStatus::FAILED);
  EXPECT_CALL(url_callback, OnRequestDone(avatar_url));

  SetupChooseCredentials(local_credentials.Pass(), federated_credentials.Pass(),
                         origin);
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());
  EXPECT_CALL(*this, OnChooseCredential(
      Field(&password_manager::CredentialInfo::type,
            password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY)));
  ManagePasswordsBubbleView::CloseBubble();
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleViewTest, AutoSignin) {
  // The switch enables the new UI.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableCredentialManagerAPI);
  ScopedVector<autofill::PasswordForm> local_credentials;
  test_form()->origin = GURL("https://example.com");
  test_form()->display_name = base::ASCIIToUTF16("Peter");
  test_form()->username_value = base::ASCIIToUTF16("pet12@gmail.com");
  GURL avatar_url("https://google.com/avatar.png");
  test_form()->avatar_url = avatar_url;
  local_credentials.push_back(new autofill::PasswordForm(*test_form()));

  // Prepare to capture the network request.
  TestURLFetcherCallback url_callback;
  net::FakeURLFetcherFactory factory(
      NULL,
      base::Bind(&TestURLFetcherCallback::CreateURLFetcher,
                 base::Unretained(&url_callback)));
  factory.SetFakeResponse(avatar_url, std::string(), net::HTTP_OK,
                          net::URLRequestStatus::FAILED);
  EXPECT_CALL(url_callback, OnRequestDone(avatar_url));

  SetupAutoSignin(local_credentials.Pass());
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());
  ::testing::Mock::VerifyAndClearExpectations(&url_callback);

  ManagePasswordsBubbleView::CloseBubble();
  EXPECT_FALSE(ManagePasswordsBubbleView::IsShowing());
  content::RunAllPendingInMessageLoop();

  // Open the bubble to manage accounts.
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, GetController()->state());
  EXPECT_CALL(url_callback, OnRequestDone(avatar_url));
  ManagePasswordsBubbleView::ShowBubble(
        browser()->tab_strip_model()->GetActiveWebContents(),
        ManagePasswordsBubble::USER_ACTION);
  EXPECT_TRUE(ManagePasswordsBubbleView::IsShowing());
}
