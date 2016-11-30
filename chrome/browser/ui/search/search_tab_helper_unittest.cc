// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/ui/search/search_ipc_router.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_sink.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

class OmniboxView;

using testing::Return;

namespace {

class MockSearchIPCRouterDelegate : public SearchIPCRouter::Delegate {
 public:
  virtual ~MockSearchIPCRouterDelegate() {}

  MOCK_METHOD1(OnInstantSupportDetermined, void(bool supports_instant));
  MOCK_METHOD1(FocusOmnibox, void(OmniboxFocusState state));
  MOCK_METHOD2(NavigateToURL, void(const GURL&, WindowOpenDisposition));
  MOCK_METHOD1(OnDeleteMostVisitedItem, void(const GURL& url));
  MOCK_METHOD1(OnUndoMostVisitedDeletion, void(const GURL& url));
  MOCK_METHOD0(OnUndoAllMostVisitedDeletions, void());
  MOCK_METHOD2(OnLogEvent, void(NTPLoggingEventType event,
                                base::TimeDelta time));
  MOCK_METHOD2(OnLogMostVisitedImpression,
               void(int position, NTPLoggingTileSource tile_source));
  MOCK_METHOD2(OnLogMostVisitedNavigation,
               void(int position, NTPLoggingTileSource tile_source));
  MOCK_METHOD1(PasteIntoOmnibox, void(const base::string16&));
  MOCK_METHOD1(OnChromeIdentityCheck, void(const base::string16& identity));
  MOCK_METHOD0(OnHistorySyncCheck, void());
};

}  // namespace

class SearchTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SearchTabHelper::CreateForWebContents(web_contents());
  }

  content::BrowserContext* CreateBrowserContext() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              BuildFakeSigninManagerBase);
    builder.AddTestingFactory(ProfileSyncServiceFactory::GetInstance(),
                              BuildMockProfileSyncService);
    return builder.Build().release();
  }

  // Creates a sign-in manager for tests.  If |username| is not empty, the
  // testing profile of the WebContents will be connected to the given account.
  void CreateSigninManager(const std::string& username) {
    SigninManagerBase* signin_manager = static_cast<SigninManagerBase*>(
        SigninManagerFactory::GetForProfile(profile()));

    if (!username.empty()) {
      ASSERT_TRUE(signin_manager);
      signin_manager->SetAuthenticatedAccountInfo(username, username);
    }
  }

  // Configure the account to |sync_history| or not.
  void SetHistorySync(bool sync_history) {
    browser_sync::ProfileSyncServiceMock* sync_service =
        static_cast<browser_sync::ProfileSyncServiceMock*>(
            ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile()));

    syncer::ModelTypeSet result;
    if (sync_history) {
      result.Put(syncer::HISTORY_DELETE_DIRECTIVES);
    }
    EXPECT_CALL(*sync_service, GetPreferredDataTypes())
        .WillRepeatedly(Return(result));
  }

  bool MessageWasSent(uint32_t id) {
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
  search_tab_helper->ipc_router().set_delegate_for_testing(mock_delegate());
  search_tab_helper->DetermineIfPageSupportsInstant();
}

TEST_F(SearchTabHelperTest, DetermineIfPageSupportsInstant_NonLocal) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();
  EXPECT_CALL(*mock_delegate(), OnInstantSupportDetermined(true)).Times(1);

  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  search_tab_helper->ipc_router().set_delegate_for_testing(mock_delegate());
  search_tab_helper->DetermineIfPageSupportsInstant();
  ASSERT_TRUE(MessageWasSent(ChromeViewMsg_DetermineIfPageSupportsInstant::ID));

  std::unique_ptr<IPC::Message> response(
      new ChromeViewHostMsg_InstantSupportDetermined(
          web_contents()->GetRenderViewHost()->GetRoutingID(),
          search_tab_helper->ipc_router().page_seq_no_for_testing(), true));
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
  search_tab_helper->ipc_router().set_delegate_for_testing(mock_delegate());
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
  EXPECT_EQ(test_identity, std::get<0>(params));
  ASSERT_TRUE(std::get<1>(params));
}

TEST_F(SearchTabHelperTest, OnChromeIdentityCheckMatchSlightlyDifferentGmail) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  CreateSigninManager(std::string("foobar123@gmail.com"));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  // For gmail, canonicalization is done so that email addresses have a
  // standard form.
  const base::string16 test_identity =
      base::ASCIIToUTF16("Foo.Bar.123@gmail.com");
  search_tab_helper->OnChromeIdentityCheck(test_identity);

  const IPC::Message* message = process()->sink().GetUniqueMessageMatching(
      ChromeViewMsg_ChromeIdentityCheckResult::ID);
  ASSERT_TRUE(message != NULL);

  ChromeViewMsg_ChromeIdentityCheckResult::Param params;
  ChromeViewMsg_ChromeIdentityCheckResult::Read(message, &params);
  EXPECT_EQ(test_identity, std::get<0>(params));
  ASSERT_TRUE(std::get<1>(params));
}

TEST_F(SearchTabHelperTest, OnChromeIdentityCheckMatchSlightlyDifferentGmail2) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  //
  CreateSigninManager(std::string("chrome.user.7FOREVER"));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  // For gmail/googlemail, canonicalization is done so that email addresses have
  // a standard form.
  const base::string16 test_identity =
      base::ASCIIToUTF16("chromeuser7forever@googlemail.com");
  search_tab_helper->OnChromeIdentityCheck(test_identity);

  const IPC::Message* message = process()->sink().GetUniqueMessageMatching(
      ChromeViewMsg_ChromeIdentityCheckResult::ID);
  ASSERT_TRUE(message != NULL);

  ChromeViewMsg_ChromeIdentityCheckResult::Param params;
  ChromeViewMsg_ChromeIdentityCheckResult::Read(message, &params);
  EXPECT_EQ(test_identity, std::get<0>(params));
  ASSERT_TRUE(std::get<1>(params));
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
  EXPECT_EQ(test_identity, std::get<0>(params));
  ASSERT_FALSE(std::get<1>(params));
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
  EXPECT_EQ(test_identity, std::get<0>(params));
  ASSERT_FALSE(std::get<1>(params));
}

TEST_F(SearchTabHelperTest, OnHistorySyncCheckSyncing) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetHistorySync(true);
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  search_tab_helper->OnHistorySyncCheck();

  const IPC::Message* message = process()->sink().GetUniqueMessageMatching(
      ChromeViewMsg_HistorySyncCheckResult::ID);
  ASSERT_TRUE(message != NULL);

  ChromeViewMsg_HistorySyncCheckResult::Param params;
  ChromeViewMsg_HistorySyncCheckResult::Read(message, &params);
  ASSERT_TRUE(std::get<0>(params));
}

TEST_F(SearchTabHelperTest, OnHistorySyncCheckNotSyncing) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetHistorySync(false);
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  search_tab_helper->OnHistorySyncCheck();

  const IPC::Message* message = process()->sink().GetUniqueMessageMatching(
      ChromeViewMsg_HistorySyncCheckResult::ID);
  ASSERT_TRUE(message != NULL);

  ChromeViewMsg_HistorySyncCheckResult::Param params;
  ChromeViewMsg_HistorySyncCheckResult::Read(message, &params);
  ASSERT_FALSE(std::get<0>(params));
}

class TabTitleObserver : public content::WebContentsObserver {
 public:
  explicit TabTitleObserver(content::WebContents* contents)
      : WebContentsObserver(contents) {}

  base::string16 title_on_start() { return title_on_start_; }
  base::string16 title_on_commit() { return title_on_commit_; }

 private:
  void DidStartProvisionalLoadForFrame(
      content::RenderFrameHost* /* render_frame_host */,
      const GURL& /* validated_url */,
      bool /* is_error_page */) override {
    title_on_start_ = web_contents()->GetTitle();
  }

  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& /* details */,
      const content::FrameNavigateParams& /* params */) override {
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
