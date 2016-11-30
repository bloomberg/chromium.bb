// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router.h"

#include <stdint.h>

#include <memory>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_start.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

class MockSearchIPCRouterDelegate : public SearchIPCRouter::Delegate {
 public:
  virtual ~MockSearchIPCRouterDelegate() {}

  MOCK_METHOD1(OnInstantSupportDetermined, void(bool supports_instant));
  MOCK_METHOD1(FocusOmnibox, void(OmniboxFocusState state));
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

class MockSearchIPCRouterPolicy : public SearchIPCRouter::Policy {
 public:
  virtual ~MockSearchIPCRouterPolicy() {}

  MOCK_METHOD1(ShouldProcessFocusOmnibox, bool(bool));
  MOCK_METHOD0(ShouldProcessDeleteMostVisitedItem, bool());
  MOCK_METHOD0(ShouldProcessUndoMostVisitedDeletion, bool());
  MOCK_METHOD0(ShouldProcessUndoAllMostVisitedDeletions, bool());
  MOCK_METHOD0(ShouldProcessLogEvent, bool());
  MOCK_METHOD1(ShouldProcessPasteIntoOmnibox, bool(bool));
  MOCK_METHOD0(ShouldProcessChromeIdentityCheck, bool());
  MOCK_METHOD0(ShouldProcessHistorySyncCheck, bool());
  MOCK_METHOD0(ShouldSendSetSuggestionToPrefetch, bool());
  MOCK_METHOD1(ShouldSendSetInputInProgress, bool(bool));
  MOCK_METHOD0(ShouldSendOmniboxFocusChanged, bool());
  MOCK_METHOD0(ShouldSendMostVisitedItems, bool());
  MOCK_METHOD0(ShouldSendThemeBackgroundInfo, bool());
  MOCK_METHOD0(ShouldSubmitQuery, bool());
};

}  // namespace

class SearchIPCRouterTest : public BrowserWithTestWindowTest {
 public:
  SearchIPCRouterTest() : field_trial_list_(NULL) {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("chrome://blank"));
    SearchTabHelper::CreateForWebContents(web_contents());

    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        &TemplateURLServiceFactory::BuildInstanceFor);
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16("foo.com"));
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.instant_url = "http://foo.com/instant?foo=foo#foo=foo&espv";
    data.new_tab_url = "https://foo.com/newtab?espv";
    data.alternate_urls.push_back("http://foo.com/alt#quux={searchTerms}");
    data.search_terms_replacement_key = "espv";

    TemplateURL* template_url =
        template_url_service->Add(base::MakeUnique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
    process()->sink().ClearMessages();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::MockRenderProcessHost* process() {
    return static_cast<content::MockRenderProcessHost*>(
        web_contents()->GetRenderViewHost()->GetProcess());
  }

  SearchTabHelper* GetSearchTabHelper(
      content::WebContents* web_contents) {
    EXPECT_NE(static_cast<content::WebContents*>(NULL), web_contents);
    return SearchTabHelper::FromWebContents(web_contents);
  }

  void SetupMockDelegateAndPolicy() {
    content::WebContents* contents = web_contents();
    ASSERT_NE(static_cast<content::WebContents*>(NULL), contents);
    SearchTabHelper* search_tab_helper = GetSearchTabHelper(contents);
    ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
    search_tab_helper->ipc_router().set_delegate_for_testing(mock_delegate());
    search_tab_helper->ipc_router().set_policy_for_testing(
        base::WrapUnique(new MockSearchIPCRouterPolicy));
  }

  bool MessageWasSent(uint32_t id) {
    return process()->sink().GetFirstMessageMatching(id) != NULL;
  }

  MockSearchIPCRouterDelegate* mock_delegate() { return &delegate_; }

  MockSearchIPCRouterPolicy* GetSearchIPCRouterPolicy() {
    content::WebContents* contents = web_contents();
    EXPECT_NE(static_cast<content::WebContents*>(NULL), contents);
    SearchTabHelper* search_tab_helper = GetSearchTabHelper(contents);
    EXPECT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
    return static_cast<MockSearchIPCRouterPolicy*>(
        search_tab_helper->ipc_router().policy_for_testing());
  }

  SearchIPCRouter& GetSearchIPCRouter() {
    return GetSearchTabHelper(web_contents())->ipc_router();
  }

  int GetSearchIPCRouterSeqNo() {
    return GetSearchIPCRouter().page_seq_no_for_testing();
  }

  void OnMessageReceived(const IPC::Message& message) {
    bool should_handle_message =
        search::IsRenderedInInstantProcess(web_contents(), profile());
    bool handled = GetSearchIPCRouter().OnMessageReceived(message);
    ASSERT_EQ(should_handle_message, handled);
  }

  bool OnSpuriousMessageReceived(const IPC::Message& message) {
    return GetSearchIPCRouter().OnMessageReceived(message);
  }

  bool IsActiveTab(content::WebContents* contents) {
    return GetSearchTabHelper(contents)->ipc_router().is_active_tab_;
  }

 private:
  MockSearchIPCRouterDelegate delegate_;
  base::FieldTrialList field_trial_list_;
};

TEST_F(SearchIPCRouterTest, IgnoreMessagesFromNonInstantRenderers) {
  NavigateAndCommitActiveTab(GURL("file://foo/bar"));
  SetupMockDelegateAndPolicy();
  EXPECT_CALL(*mock_delegate(), FocusOmnibox(OMNIBOX_FOCUS_VISIBLE)).Times(0);
  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  EXPECT_TRUE(is_active_tab);

  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldProcessFocusOmnibox(is_active_tab)).Times(0);

  OnMessageReceived(ChromeViewHostMsg_FocusOmnibox(
      contents->GetRenderViewHost()->GetRoutingID(), GetSearchIPCRouterSeqNo(),
      OMNIBOX_FOCUS_VISIBLE));
}

TEST_F(SearchIPCRouterTest, ProcessFocusOmniboxMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), FocusOmnibox(OMNIBOX_FOCUS_VISIBLE)).Times(1);

  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  EXPECT_TRUE(is_active_tab);
  EXPECT_CALL(*policy, ShouldProcessFocusOmnibox(is_active_tab)).Times(1)
      .WillOnce(testing::Return(true));

  OnMessageReceived(ChromeViewHostMsg_FocusOmnibox(
      contents->GetRenderViewHost()->GetRoutingID(), GetSearchIPCRouterSeqNo(),
      OMNIBOX_FOCUS_VISIBLE));
}

TEST_F(SearchIPCRouterTest, IgnoreFocusOmniboxMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), FocusOmnibox(OMNIBOX_FOCUS_VISIBLE)).Times(0);

  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  EXPECT_TRUE(is_active_tab);
  EXPECT_CALL(*policy, ShouldProcessFocusOmnibox(is_active_tab)).Times(1)
      .WillOnce(testing::Return(false));

  OnMessageReceived(ChromeViewHostMsg_FocusOmnibox(
      contents->GetRenderViewHost()->GetRoutingID(), GetSearchIPCRouterSeqNo(),
      OMNIBOX_FOCUS_VISIBLE));
}

TEST_F(SearchIPCRouterTest, HandleTabChangedEvents) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  content::WebContents* contents = web_contents();
  EXPECT_EQ(0, browser()->tab_strip_model()->GetIndexOfWebContents(contents));
  EXPECT_TRUE(IsActiveTab(contents));

  // Add a new tab to deactivate the current tab.
  AddTab(browser(), GURL(url::kAboutBlankURL));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->GetIndexOfWebContents(contents));
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_FALSE(IsActiveTab(contents));

  // Activate the first tab.
  browser()->tab_strip_model()->ActivateTabAt(1, false);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(),
            browser()->tab_strip_model()->GetIndexOfWebContents(contents));
  EXPECT_TRUE(IsActiveTab(contents));
}

TEST_F(SearchIPCRouterTest, ProcessLogEventMsg) {
  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(123);
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnLogEvent(NTP_ALL_TILES_LOADED, delta))
      .Times(1);
  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(1)
      .WillOnce(testing::Return(true));

  content::WebContents* contents = web_contents();
  OnMessageReceived(ChromeViewHostMsg_LogEvent(
      contents->GetRenderViewHost()->GetRoutingID(), GetSearchIPCRouterSeqNo(),
      NTP_ALL_TILES_LOADED, delta));
}

TEST_F(SearchIPCRouterTest, IgnoreLogEventMsg) {
  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(123);
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnLogEvent(NTP_ALL_TILES_LOADED, delta))
      .Times(0);
  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(1)
      .WillOnce(testing::Return(false));

  content::WebContents* contents = web_contents();
  OnMessageReceived(ChromeViewHostMsg_LogEvent(
      contents->GetRenderViewHost()->GetRoutingID(), GetSearchIPCRouterSeqNo(),
      NTP_ALL_TILES_LOADED, delta));
}

TEST_F(SearchIPCRouterTest, ProcessLogMostVisitedImpressionMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(),
      OnLogMostVisitedImpression(3, NTPLoggingTileSource::SERVER)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(1)
      .WillOnce(testing::Return(true));

  content::WebContents* contents = web_contents();
  OnMessageReceived(ChromeViewHostMsg_LogMostVisitedImpression(
      contents->GetRenderViewHost()->GetRoutingID(), GetSearchIPCRouterSeqNo(),
      3, NTPLoggingTileSource::SERVER));
}

TEST_F(SearchIPCRouterTest, ProcessLogMostVisitedNavigationMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(),
      OnLogMostVisitedNavigation(3, NTPLoggingTileSource::SERVER)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(1)
      .WillOnce(testing::Return(true));

  content::WebContents* contents = web_contents();
  OnMessageReceived(ChromeViewHostMsg_LogMostVisitedNavigation(
      contents->GetRenderViewHost()->GetRoutingID(), GetSearchIPCRouterSeqNo(),
      3, NTPLoggingTileSource::SERVER));
}

TEST_F(SearchIPCRouterTest, ProcessChromeIdentityCheckMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  const base::string16 test_identity = base::ASCIIToUTF16("foo@bar.com");
  EXPECT_CALL(*mock_delegate(), OnChromeIdentityCheck(test_identity)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessChromeIdentityCheck()).Times(1)
      .WillOnce(testing::Return(true));

  content::WebContents* contents = web_contents();
  OnMessageReceived(ChromeViewHostMsg_ChromeIdentityCheck(
      contents->GetRenderViewHost()->GetRoutingID(), GetSearchIPCRouterSeqNo(),
      test_identity));
}

TEST_F(SearchIPCRouterTest, IgnoreChromeIdentityCheckMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();

  const base::string16 test_identity = base::ASCIIToUTF16("foo@bar.com");
  EXPECT_CALL(*mock_delegate(), OnChromeIdentityCheck(test_identity)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessChromeIdentityCheck()).Times(1)
      .WillOnce(testing::Return(false));

  content::WebContents* contents = web_contents();
  OnMessageReceived(ChromeViewHostMsg_ChromeIdentityCheck(
      contents->GetRenderViewHost()->GetRoutingID(), GetSearchIPCRouterSeqNo(),
      test_identity));
}

TEST_F(SearchIPCRouterTest, ProcessHistorySyncCheckMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnHistorySyncCheck()).Times(1);
  EXPECT_CALL(*policy, ShouldProcessHistorySyncCheck()).Times(1)
      .WillOnce(testing::Return(true));

  content::WebContents* contents = web_contents();
  OnMessageReceived(ChromeViewHostMsg_HistorySyncCheck(
      contents->GetRenderViewHost()->GetRoutingID(),
      GetSearchIPCRouterSeqNo()));
}

TEST_F(SearchIPCRouterTest, IgnoreHistorySyncCheckMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();

  EXPECT_CALL(*mock_delegate(), OnHistorySyncCheck()).Times(0);
  EXPECT_CALL(*policy, ShouldProcessHistorySyncCheck()).Times(1)
      .WillOnce(testing::Return(false));

  content::WebContents* contents = web_contents();
  OnMessageReceived(ChromeViewHostMsg_HistorySyncCheck(
      contents->GetRenderViewHost()->GetRoutingID(),
      GetSearchIPCRouterSeqNo()));
}

TEST_F(SearchIPCRouterTest, ProcessDeleteMostVisitedItemMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnDeleteMostVisitedItem(item_url)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessDeleteMostVisitedItem()).Times(1)
      .WillOnce(testing::Return(true));

  content::WebContents* contents = web_contents();
  OnMessageReceived(ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(
      contents->GetRenderViewHost()->GetRoutingID(), GetSearchIPCRouterSeqNo(),
      item_url));
}

TEST_F(SearchIPCRouterTest, IgnoreDeleteMostVisitedItemMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnDeleteMostVisitedItem(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessDeleteMostVisitedItem()).Times(1)
      .WillOnce(testing::Return(false));

  content::WebContents* contents = web_contents();
  OnMessageReceived(ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(
      contents->GetRenderViewHost()->GetRoutingID(), GetSearchIPCRouterSeqNo(),
      item_url));
}

TEST_F(SearchIPCRouterTest, ProcessUndoMostVisitedDeletionMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnUndoMostVisitedDeletion(item_url)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessUndoMostVisitedDeletion()).Times(1)
      .WillOnce(testing::Return(true));

  content::WebContents* contents = web_contents();
  OnMessageReceived(ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(
      contents->GetRenderViewHost()->GetRoutingID(), GetSearchIPCRouterSeqNo(),
      item_url));
}

TEST_F(SearchIPCRouterTest, IgnoreUndoMostVisitedDeletionMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnUndoMostVisitedDeletion(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoMostVisitedDeletion()).Times(1)
      .WillOnce(testing::Return(false));

  content::WebContents* contents = web_contents();
  OnMessageReceived(ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(
      contents->GetRenderViewHost()->GetRoutingID(), GetSearchIPCRouterSeqNo(),
      item_url));
}

TEST_F(SearchIPCRouterTest, ProcessUndoAllMostVisitedDeletionsMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnUndoAllMostVisitedDeletions()).Times(1);
  EXPECT_CALL(*policy, ShouldProcessUndoAllMostVisitedDeletions()).Times(1)
      .WillOnce(testing::Return(true));

  content::WebContents* contents = web_contents();
  OnMessageReceived(ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
      contents->GetRenderViewHost()->GetRoutingID(),
      GetSearchIPCRouterSeqNo()));
}

TEST_F(SearchIPCRouterTest, IgnoreUndoAllMostVisitedDeletionsMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnUndoAllMostVisitedDeletions()).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoAllMostVisitedDeletions()).Times(1)
      .WillOnce(testing::Return(false));

  content::WebContents* contents = web_contents();
  OnMessageReceived(ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
      contents->GetRenderViewHost()->GetRoutingID(),
      GetSearchIPCRouterSeqNo()));
}

TEST_F(SearchIPCRouterTest, IgnoreMessageIfThePageIsNotActive) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  int page_seq_no = GetSearchIPCRouterSeqNo();

  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  GURL item_url("www.foo.com");

  // Navigate away from the NTP. Afterwards, all messages should be ignored.
  NavigateAndCommitActiveTab(item_url);

  EXPECT_CALL(*mock_delegate(), OnDeleteMostVisitedItem(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessDeleteMostVisitedItem()).Times(0);
  OnMessageReceived(ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(
      contents->GetRenderViewHost()->GetRoutingID(), page_seq_no, item_url));

  EXPECT_CALL(*mock_delegate(), OnUndoMostVisitedDeletion(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoMostVisitedDeletion()).Times(0);
  OnMessageReceived(ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(
      contents->GetRenderViewHost()->GetRoutingID(), page_seq_no, item_url));

  EXPECT_CALL(*mock_delegate(), OnUndoAllMostVisitedDeletions()).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoAllMostVisitedDeletions()).Times(0);
  OnMessageReceived(ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
      contents->GetRenderViewHost()->GetRoutingID(), page_seq_no));

  EXPECT_CALL(*mock_delegate(), FocusOmnibox(OMNIBOX_FOCUS_VISIBLE)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessFocusOmnibox(is_active_tab)).Times(0);
  OnMessageReceived(ChromeViewHostMsg_FocusOmnibox(
      contents->GetRenderViewHost()->GetRoutingID(), page_seq_no,
      OMNIBOX_FOCUS_VISIBLE));

  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(123);
  EXPECT_CALL(*mock_delegate(), OnLogEvent(NTP_ALL_TILES_LOADED, delta))
      .Times(0);
  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(0);
  OnMessageReceived(
      ChromeViewHostMsg_LogEvent(contents->GetRenderViewHost()->GetRoutingID(),
                                 page_seq_no, NTP_ALL_TILES_LOADED, delta));

  base::string16 text;
  EXPECT_CALL(*mock_delegate(), PasteIntoOmnibox(text)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessPasteIntoOmnibox(is_active_tab)).Times(0);
  OnMessageReceived(ChromeViewHostMsg_PasteAndOpenDropdown(
      contents->GetRenderViewHost()->GetRoutingID(), page_seq_no, text));
}

TEST_F(SearchIPCRouterTest, ProcessPasteAndOpenDropdownMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();

  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  EXPECT_TRUE(is_active_tab);

  base::string16 text;
  EXPECT_CALL(*mock_delegate(), PasteIntoOmnibox(text)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessPasteIntoOmnibox(is_active_tab)).Times(1)
      .WillOnce(testing::Return(true));
  OnMessageReceived(ChromeViewHostMsg_PasteAndOpenDropdown(
      contents->GetRenderViewHost()->GetRoutingID(), GetSearchIPCRouterSeqNo(),
      text));
}

TEST_F(SearchIPCRouterTest, IgnorePasteAndOpenDropdownMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  base::string16 text;
  EXPECT_CALL(*mock_delegate(), PasteIntoOmnibox(text)).Times(0);

  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  EXPECT_TRUE(is_active_tab);

  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldProcessPasteIntoOmnibox(is_active_tab)).Times(1)
      .WillOnce(testing::Return(false));

  OnMessageReceived(ChromeViewHostMsg_PasteAndOpenDropdown(
      contents->GetRenderViewHost()->GetRoutingID(), GetSearchIPCRouterSeqNo(),
      text));
}

TEST_F(SearchIPCRouterTest, SendSetSuggestionToPrefetch) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendSetSuggestionToPrefetch()).Times(1)
      .WillOnce(testing::Return(true));

  process()->sink().ClearMessages();
  content::WebContents* contents = web_contents();
  GetSearchTabHelper(contents)->SetSuggestionToPrefetch(InstantSuggestion());
  EXPECT_TRUE(MessageWasSent(
      ChromeViewMsg_SearchBoxSetSuggestionToPrefetch::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendSetSuggestionToPrefetch) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendSetSuggestionToPrefetch()).Times(1)
      .WillOnce(testing::Return(false));

  process()->sink().ClearMessages();
  content::WebContents* contents = web_contents();
  GetSearchTabHelper(contents)->SetSuggestionToPrefetch(InstantSuggestion());
  EXPECT_FALSE(MessageWasSent(
      ChromeViewMsg_SearchBoxSetSuggestionToPrefetch::ID));
}

TEST_F(SearchIPCRouterTest, SendOmniboxFocusChange) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendOmniboxFocusChanged()).Times(1)
      .WillOnce(testing::Return(true));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().OmniboxFocusChanged(OMNIBOX_FOCUS_NONE,
                                           OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxFocusChanged::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendOmniboxFocusChange) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendOmniboxFocusChanged()).Times(1)
      .WillOnce(testing::Return(false));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().OmniboxFocusChanged(OMNIBOX_FOCUS_NONE,
                                           OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxFocusChanged::ID));
}

TEST_F(SearchIPCRouterTest, SendSetInputInProgress) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendSetInputInProgress(true)).Times(1)
      .WillOnce(testing::Return(true));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().SetInputInProgress(true);
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxSetInputInProgress::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendSetInputInProgress) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendSetInputInProgress(true)).Times(1)
      .WillOnce(testing::Return(false));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().SetInputInProgress(true);
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxSetInputInProgress::ID));
}

TEST_F(SearchIPCRouterTest, SendMostVisitedItemsMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendMostVisitedItems()).Times(1)
      .WillOnce(testing::Return(true));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().SendMostVisitedItems(
      std::vector<InstantMostVisitedItem>());
  EXPECT_TRUE(MessageWasSent(
      ChromeViewMsg_SearchBoxMostVisitedItemsChanged::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendMostVisitedItemsMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendMostVisitedItems()).Times(1)
      .WillOnce(testing::Return(false));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().SendMostVisitedItems(
      std::vector<InstantMostVisitedItem>());
  EXPECT_FALSE(MessageWasSent(
      ChromeViewMsg_SearchBoxMostVisitedItemsChanged::ID));
}

TEST_F(SearchIPCRouterTest, SendThemeBackgroundInfoMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendThemeBackgroundInfo()).Times(1)
      .WillOnce(testing::Return(true));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().SendThemeBackgroundInfo(ThemeBackgroundInfo());
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxThemeChanged::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendThemeBackgroundInfoMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendThemeBackgroundInfo()).Times(1)
      .WillOnce(testing::Return(false));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().SendThemeBackgroundInfo(ThemeBackgroundInfo());
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxThemeChanged::ID));
}

TEST_F(SearchIPCRouterTest, SendSubmitMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSubmitQuery()).Times(1)
      .WillOnce(testing::Return(true));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().Submit(base::string16(), EmbeddedSearchRequestParams());
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxSubmit::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendSubmitMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSubmitQuery()).Times(1)
      .WillOnce(testing::Return(false));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().Submit(base::string16(), EmbeddedSearchRequestParams());
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxSubmit::ID));
}

TEST_F(SearchIPCRouterTest, SpuriousMessageTypesIgnored) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  const int routing_id = web_contents()->GetRenderViewHost()->GetRoutingID();

  // Construct a series of synthetic messages for each valid IPC message type,
  // ensuring the router ignores them all.
  for (int i = 0; i < LastIPCMsgStart; ++i) {
    const int message_id = i << 16;
    ASSERT_EQ(IPC_MESSAGE_ID_CLASS(message_id), i);
    IPC::Message msg(routing_id, message_id, IPC::Message::PRIORITY_LOW);
    EXPECT_FALSE(OnSpuriousMessageReceived(msg)) << i;
  }
}
