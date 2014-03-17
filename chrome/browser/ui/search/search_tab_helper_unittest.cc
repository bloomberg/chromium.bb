// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/search/search_ipc_router.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/ntp_logging_events.h"
#include "chrome/common/omnibox_focus_state.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_sink.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

class MockSearchIPCRouterDelegate : public SearchIPCRouter::Delegate {
 public:
  virtual ~MockSearchIPCRouterDelegate() {}

  MOCK_METHOD1(OnInstantSupportDetermined, void(bool supports_instant));
  MOCK_METHOD1(OnSetVoiceSearchSupport, void(bool supports_voice_search));
  MOCK_METHOD1(FocusOmnibox, void(OmniboxFocusState state));
  MOCK_METHOD3(NavigateToURL, void(const GURL&, WindowOpenDisposition, bool));
  MOCK_METHOD1(OnDeleteMostVisitedItem, void(const GURL& url));
  MOCK_METHOD1(OnUndoMostVisitedDeletion, void(const GURL& url));
  MOCK_METHOD0(OnUndoAllMostVisitedDeletions, void());
  MOCK_METHOD1(OnLogEvent, void(NTPLoggingEventType event));
  MOCK_METHOD2(OnLogMostVisitedImpression,
               void(int position, const base::string16& provider));
  MOCK_METHOD2(OnLogMostVisitedNavigation,
               void(int position, const base::string16& provider));
  MOCK_METHOD1(PasteIntoOmnibox, void(const base::string16&));
  MOCK_METHOD1(OnChromeIdentityCheck, void(const base::string16& identity));
};

}  // namespace

class SearchTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();
    SearchTabHelper::CreateForWebContents(web_contents());
  }

  // Creates a sign-in manager for tests.  If |username| is not empty, the
  // testing profile of the WebContents will be connected to the given account.
  void CreateSigninManager(const std::string& username) {
    SigninManagerBase* signin_manager = static_cast<SigninManagerBase*>(
        SigninManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), FakeSigninManagerBase::Build));

    if (!username.empty()) {
      ASSERT_TRUE(signin_manager);
      signin_manager->SetAuthenticatedUsername(username);
    }
  }

  bool MessageWasSent(uint32 id) {
    return process()->sink().GetFirstMessageMatching(id) != NULL;
  }

  MockSearchIPCRouterDelegate* mock_delegate() { return &delegate_; }

 private:
  MockSearchIPCRouterDelegate delegate_;
};

TEST_F(SearchTabHelperTest, DetermineIfPageSupportsInstant_Local) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_CALL(*mock_delegate(), OnInstantSupportDetermined(true)).Times(0);

  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  search_tab_helper->ipc_router().set_delegate(mock_delegate());
  search_tab_helper->DetermineIfPageSupportsInstant();
}

TEST_F(SearchTabHelperTest, DetermineIfPageSupportsInstant_NonLocal) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();
  EXPECT_CALL(*mock_delegate(), OnInstantSupportDetermined(true)).Times(1);

  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  search_tab_helper->ipc_router().set_delegate(mock_delegate());
  search_tab_helper->DetermineIfPageSupportsInstant();
  ASSERT_TRUE(MessageWasSent(ChromeViewMsg_DetermineIfPageSupportsInstant::ID));

  scoped_ptr<IPC::Message> response(
      new ChromeViewHostMsg_InstantSupportDetermined(
          web_contents()->GetRoutingID(),
          web_contents()->GetController().GetVisibleEntry()->GetPageID(),
          true));
  search_tab_helper->ipc_router().OnMessageReceived(*response);
}

TEST_F(SearchTabHelperTest, PageURLDoesntBelongToInstantRenderer) {
  // Navigate to a page URL that doesn't belong to Instant renderer.
  // SearchTabHelper::DeterminerIfPageSupportsInstant() should return
  // immediately without dispatching any message to the renderer.
  NavigateAndCommit(GURL("http://www.example.com"));
  process()->sink().ClearMessages();
  EXPECT_CALL(*mock_delegate(), OnInstantSupportDetermined(false)).Times(0);

  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  search_tab_helper->ipc_router().set_delegate(mock_delegate());
  search_tab_helper->DetermineIfPageSupportsInstant();
  ASSERT_FALSE(MessageWasSent(
      ChromeViewMsg_DetermineIfPageSupportsInstant::ID));
}

TEST_F(SearchTabHelperTest, OnChromeIdentityCheckMatch) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  CreateSigninManager(std::string("foo@bar.com"));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  const base::string16 test_identity = base::ASCIIToUTF16("foo@bar.com");
  search_tab_helper->OnChromeIdentityCheck(test_identity);

  const IPC::Message* message = process()->sink().GetUniqueMessageMatching(
      ChromeViewMsg_ChromeIdentityCheckResult::ID);
  ASSERT_TRUE(message != NULL);

  ChromeViewMsg_ChromeIdentityCheckResult::Param params;
  ChromeViewMsg_ChromeIdentityCheckResult::Read(message, &params);
  EXPECT_EQ(test_identity, params.a);
  ASSERT_TRUE(params.b);
}

TEST_F(SearchTabHelperTest, OnChromeIdentityCheckMismatch) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  CreateSigninManager(std::string("foo@bar.com"));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  const base::string16 test_identity = base::ASCIIToUTF16("bar@foo.com");
  search_tab_helper->OnChromeIdentityCheck(test_identity);

  const IPC::Message* message = process()->sink().GetUniqueMessageMatching(
      ChromeViewMsg_ChromeIdentityCheckResult::ID);
  ASSERT_TRUE(message != NULL);

  ChromeViewMsg_ChromeIdentityCheckResult::Param params;
  ChromeViewMsg_ChromeIdentityCheckResult::Read(message, &params);
  EXPECT_EQ(test_identity, params.a);
  ASSERT_FALSE(params.b);
}

TEST_F(SearchTabHelperTest, OnChromeIdentityCheckSignedOutMatch) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  // This test does not sign in.
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  const base::string16 test_identity;
  search_tab_helper->OnChromeIdentityCheck(test_identity);

  const IPC::Message* message = process()->sink().GetUniqueMessageMatching(
      ChromeViewMsg_ChromeIdentityCheckResult::ID);
  ASSERT_TRUE(message != NULL);

  ChromeViewMsg_ChromeIdentityCheckResult::Param params;
  ChromeViewMsg_ChromeIdentityCheckResult::Read(message, &params);
  EXPECT_EQ(test_identity, params.a);
  ASSERT_TRUE(params.b);
}

TEST_F(SearchTabHelperTest, OnChromeIdentityCheckSignedOutMismatch) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  // This test does not sign in.
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  const base::string16 test_identity = base::ASCIIToUTF16("bar@foo.com");
  search_tab_helper->OnChromeIdentityCheck(test_identity);

  const IPC::Message* message = process()->sink().GetUniqueMessageMatching(
      ChromeViewMsg_ChromeIdentityCheckResult::ID);
  ASSERT_TRUE(message != NULL);

  ChromeViewMsg_ChromeIdentityCheckResult::Param params;
  ChromeViewMsg_ChromeIdentityCheckResult::Read(message, &params);
  EXPECT_EQ(test_identity, params.a);
  ASSERT_FALSE(params.b);
}

class TabTitleObserver : public content::WebContentsObserver {
 public:
  explicit TabTitleObserver(content::WebContents* contents)
      : WebContentsObserver(contents) {}

  base::string16 title_on_start() { return title_on_start_; }
  base::string16 title_on_commit() { return title_on_commit_; }

 private:
  virtual void DidStartProvisionalLoadForFrame(
      int64 /* frame_id */,
      int64 /* parent_frame_id */,
      bool /* is_main_frame */,
      const GURL& /* validated_url */,
      bool /* is_error_page */,
      bool /* is_iframe_srcdoc */,
      content::RenderViewHost* /* render_view_host */) OVERRIDE {
    title_on_start_ = web_contents()->GetTitle();
  }

  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& /* details */,
      const content::FrameNavigateParams& /* params */) OVERRIDE {
    title_on_commit_ = web_contents()->GetTitle();
  }

  base::string16 title_on_start_;
  base::string16 title_on_commit_;
};

TEST_F(SearchTabHelperTest, TitleIsSetForNTP) {
  TabTitleObserver title_observer(web_contents());
  NavigateAndCommit(GURL(chrome::kChromeUINewTabURL));
  const base::string16 title = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
  EXPECT_EQ(title, title_observer.title_on_start());
  EXPECT_EQ(title, title_observer.title_on_commit());
  EXPECT_EQ(title, web_contents()->GetTitle());
}

class SearchTabHelperWindowTest : public BrowserWithTestWindowTest {
 protected:
  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &TemplateURLServiceFactory::BuildInstanceFor);
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    ui_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.instant_url = "http://foo.com/instant?"
        "{google:omniboxStartMarginParameter}{google:forceInstantResults}"
        "foo=foo#foo=foo&strk";
    data.new_tab_url = std::string("https://foo.com/newtab?strk");
    data.alternate_urls.push_back("http://foo.com/alt#quux={searchTerms}");
    data.search_terms_replacement_key = "strk";

    TemplateURL* template_url = new TemplateURL(profile(), data);
    template_url_service->Add(template_url);
    template_url_service->SetDefaultSearchProvider(template_url);
  }
};

TEST_F(SearchTabHelperWindowTest, OnProvisionalLoadFailRedirectNTPToLocal) {
  AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
  content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationController* controller = &contents->GetController();

  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(contents);
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  // A failed provisional load of a cacheable NTP should be redirected to local
  // NTP.
  const GURL cacheableNTPURL = chrome::GetNewTabPageURL(profile());
  search_tab_helper->DidFailProvisionalLoad(1, base::string16(), true,
      cacheableNTPURL, 1, base::string16(), NULL);
  CommitPendingLoad(controller);
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
                 controller->GetLastCommittedEntry()->GetURL());
}

TEST_F(SearchTabHelperWindowTest, OnProvisionalLoadFailDontRedirectIfAborted) {
  AddTab(browser(), GURL("chrome://blank"));
  content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationController* controller = &contents->GetController();

  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(contents);
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  // A failed provisional load of a cacheable NTP should be redirected to local
  // NTP.
  const GURL cacheableNTPURL = chrome::GetNewTabPageURL(profile());
  search_tab_helper->DidFailProvisionalLoad(1, base::string16(), true,
      cacheableNTPURL, net::ERR_ABORTED, base::string16(), NULL);
  CommitPendingLoad(controller);
  EXPECT_EQ(GURL("chrome://blank"),
                 controller->GetLastCommittedEntry()->GetURL());
}

TEST_F(SearchTabHelperWindowTest, OnProvisionalLoadFailDontRedirectNonNTP) {
  AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
  content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationController* controller = &contents->GetController();

  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(contents);
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  // Any other web page shouldn't be redirected when provisional load fails.
  search_tab_helper->DidFailProvisionalLoad(1, base::string16(), true,
      GURL("http://www.example.com"), 1, base::string16(), NULL);
  CommitPendingLoad(controller);
  EXPECT_NE(GURL(chrome::kChromeSearchLocalNtpUrl),
                 controller->GetLastCommittedEntry()->GetURL());
}
