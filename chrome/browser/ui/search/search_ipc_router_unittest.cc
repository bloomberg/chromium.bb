// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router.h"

#include <vector>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/ntp_logging_events.h"
#include "chrome/common/omnibox_focus_state.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class MockSearchIPCRouterDelegate : public SearchIPCRouter::Delegate {
 public:
  virtual ~MockSearchIPCRouterDelegate() {}

  MOCK_METHOD1(OnInstantSupportDetermined, void(bool supports_instant));
  MOCK_METHOD1(OnSetVoiceSearchSupport, void(bool supports_voice_search));
  MOCK_METHOD1(FocusOmnibox, void(OmniboxFocusState state));
  MOCK_METHOD1(OnDeleteMostVisitedItem, void(const GURL& url));
  MOCK_METHOD1(OnUndoMostVisitedDeletion, void(const GURL& url));
  MOCK_METHOD0(OnUndoAllMostVisitedDeletions, void());
  MOCK_METHOD1(OnLogEvent, void(NTPLoggingEventType event));
};

class MockSearchIPCRouterPolicy : public SearchIPCRouter::Policy {
 public:
  virtual ~MockSearchIPCRouterPolicy() {}

  MOCK_METHOD0(ShouldProcessSetVoiceSearchSupport, bool());
  MOCK_METHOD1(ShouldProcessFocusOmnibox, bool(bool));
  MOCK_METHOD0(ShouldProcessDeleteMostVisitedItem, bool());
  MOCK_METHOD0(ShouldProcessUndoMostVisitedDeletion, bool());
  MOCK_METHOD0(ShouldProcessUndoAllMostVisitedDeletions, bool());
  MOCK_METHOD0(ShouldProcessLogEvent, bool());
  MOCK_METHOD0(ShouldSendSetPromoInformation, bool());
  MOCK_METHOD0(ShouldSendSetDisplayInstantResults, bool());
  MOCK_METHOD0(ShouldSendSetSuggestionToPrefetch, bool());
  MOCK_METHOD0(ShouldSendMostVisitedItems, bool());
  MOCK_METHOD0(ShouldSendThemeBackgroundInfo, bool());
  MOCK_METHOD0(ShouldSubmitQuery, bool());
};

}  // namespace

class SearchIPCRouterTest : public BrowserWithTestWindowTest {
 public:
  virtual void SetUp() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableInstantExtendedAPI);
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("chrome://blank"));
    SearchTabHelper::CreateForWebContents(web_contents());
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

  void SetupMockDelegateAndPolicy(content::WebContents* web_contents) {
    ASSERT_NE(static_cast<content::WebContents*>(NULL), web_contents);
    SearchTabHelper* search_tab_helper =
        GetSearchTabHelper(web_contents);
    ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
    search_tab_helper->ipc_router().set_delegate(mock_delegate());
    search_tab_helper->ipc_router().set_policy(
        make_scoped_ptr(new MockSearchIPCRouterPolicy)
            .PassAs<SearchIPCRouter::Policy>());
  }

  bool MessageWasSent(uint32 id) {
    return process()->sink().GetFirstMessageMatching(id) != NULL;
  }

  MockSearchIPCRouterDelegate* mock_delegate() { return &delegate_; }

  MockSearchIPCRouterPolicy* GetSearchIPCRouterPolicy(
      content::WebContents* web_contents) {
    EXPECT_NE(static_cast<content::WebContents*>(NULL), web_contents);
    SearchTabHelper* search_tab_helper =
        GetSearchTabHelper(web_contents);
    EXPECT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
    return static_cast<MockSearchIPCRouterPolicy*>(
        search_tab_helper->ipc_router().policy());
  }

 private:
  MockSearchIPCRouterDelegate delegate_;
};

TEST_F(SearchIPCRouterTest, ProcessVoiceSearchSupportMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*mock_delegate(), OnSetVoiceSearchSupport(true)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessSetVoiceSearchSupport()).Times(1)
      .WillOnce(testing::Return(true));

  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SetVoiceSearchSupported(
          contents->GetRoutingID(),
          contents->GetController().GetVisibleEntry()->GetPageID(),
          true));
  GetSearchTabHelper(contents)->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreVoiceSearchSupportMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();
  EXPECT_CALL(*mock_delegate(), OnSetVoiceSearchSupport(true)).Times(0);

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*policy, ShouldProcessSetVoiceSearchSupport()).Times(1)
      .WillOnce(testing::Return(false));

  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SetVoiceSearchSupported(
          contents->GetRoutingID(),
          contents->GetController().GetVisibleEntry()->GetPageID(),
          true));
  GetSearchTabHelper(contents)->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessFocusOmniboxMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*mock_delegate(), FocusOmnibox(OMNIBOX_FOCUS_VISIBLE)).Times(1);

  SearchTabHelper* search_tab_helper = GetSearchTabHelper(contents);
  bool is_active_tab = search_tab_helper->ipc_router().is_active_tab_;
  EXPECT_TRUE(is_active_tab);
  EXPECT_CALL(*policy, ShouldProcessFocusOmnibox(is_active_tab)).Times(1)
      .WillOnce(testing::Return(true));

  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_FocusOmnibox(
      contents->GetRoutingID(),
      contents->GetController().GetVisibleEntry()->GetPageID(),
      OMNIBOX_FOCUS_VISIBLE));
  search_tab_helper->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreFocusOmniboxMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*mock_delegate(), FocusOmnibox(OMNIBOX_FOCUS_VISIBLE)).Times(0);

  SearchTabHelper* search_tab_helper = GetSearchTabHelper(contents);
  bool is_active_tab = search_tab_helper->ipc_router().is_active_tab_;
  EXPECT_TRUE(is_active_tab);
  EXPECT_CALL(*policy, ShouldProcessFocusOmnibox(is_active_tab)).Times(1)
      .WillOnce(testing::Return(false));

  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_FocusOmnibox(
      contents->GetRoutingID(),
      contents->GetController().GetVisibleEntry()->GetPageID(),
      OMNIBOX_FOCUS_VISIBLE));
  search_tab_helper->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, HandleTabChangedEvents) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  content::WebContents* contents = web_contents();
  EXPECT_EQ(0, browser()->tab_strip_model()->GetIndexOfWebContents(contents));
  SearchTabHelper* search_tab_helper = GetSearchTabHelper(contents);
  EXPECT_TRUE(search_tab_helper->ipc_router().is_active_tab_);

  // Add a new tab to deactivate the current tab.
  AddTab(browser(), GURL(content::kAboutBlankURL));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->GetIndexOfWebContents(contents));
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_FALSE(search_tab_helper->ipc_router().is_active_tab_);

  // Activate the first tab.
  browser()->tab_strip_model()->ActivateTabAt(1, false);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(),
            browser()->tab_strip_model()->GetIndexOfWebContents(contents));
  EXPECT_TRUE(search_tab_helper->ipc_router().is_active_tab_);
}

TEST_F(SearchIPCRouterTest, ProcessLogEventMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();
  EXPECT_CALL(*mock_delegate(), OnLogEvent(NTP_MOUSEOVER)).Times(1);

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);

  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(1)
      .WillOnce(testing::Return(true));

  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_LogEvent(
      contents->GetRoutingID(),
      contents->GetController().GetVisibleEntry()->GetPageID(),
      NTP_MOUSEOVER));
  GetSearchTabHelper(contents)->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreLogEventMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();
  EXPECT_CALL(*mock_delegate(), OnLogEvent(NTP_MOUSEOVER)).Times(0);

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(1)
      .WillOnce(testing::Return(false));

  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_LogEvent(
      contents->GetRoutingID(),
      contents->GetController().GetVisibleEntry()->GetPageID(),
      NTP_MOUSEOVER));
  GetSearchTabHelper(contents)->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessDeleteMostVisitedItemMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);

  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnDeleteMostVisitedItem(item_url)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessDeleteMostVisitedItem()).Times(1)
      .WillOnce(testing::Return(true));

  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(
          contents->GetRoutingID(),
          contents->GetController().GetVisibleEntry()->GetPageID(),
          item_url));
  GetSearchTabHelper(contents)->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreDeleteMostVisitedItemMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);

  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnDeleteMostVisitedItem(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessDeleteMostVisitedItem()).Times(1)
      .WillOnce(testing::Return(false));

  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(
          contents->GetRoutingID(),
          contents->GetController().GetVisibleEntry()->GetPageID(),
          item_url));
  GetSearchTabHelper(contents)->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessUndoMostVisitedDeletionMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnUndoMostVisitedDeletion(item_url)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessUndoMostVisitedDeletion()).Times(1)
      .WillOnce(testing::Return(true));

  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(
          contents->GetRoutingID(),
          contents->GetController().GetVisibleEntry()->GetPageID(),
          item_url));
  GetSearchTabHelper(contents)->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreUndoMostVisitedDeletionMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnUndoMostVisitedDeletion(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoMostVisitedDeletion()).Times(1)
      .WillOnce(testing::Return(false));

  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(
          contents->GetRoutingID(),
          contents->GetController().GetVisibleEntry()->GetPageID(),
          item_url));
  GetSearchTabHelper(contents)->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessUndoAllMostVisitedDeletionsMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*mock_delegate(), OnUndoAllMostVisitedDeletions()).Times(1);
  EXPECT_CALL(*policy, ShouldProcessUndoAllMostVisitedDeletions()).Times(1)
      .WillOnce(testing::Return(true));

  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
          contents->GetRoutingID(),
          contents->GetController().GetVisibleEntry()->GetPageID()));
  GetSearchTabHelper(contents)->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreUndoAllMostVisitedDeletionsMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*mock_delegate(), OnUndoAllMostVisitedDeletions()).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoAllMostVisitedDeletions()).Times(1)
      .WillOnce(testing::Return(false));

  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
          contents->GetRoutingID(),
          contents->GetController().GetVisibleEntry()->GetPageID()));
  GetSearchTabHelper(contents)->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreMessageIfThePageIsNotActive) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);

  SearchTabHelper* search_tab_helper = GetSearchTabHelper(contents);
  int invalid_page_id = 1000;
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnDeleteMostVisitedItem(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessDeleteMostVisitedItem()).Times(0);
  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(
          contents->GetRoutingID(), invalid_page_id, item_url));
  search_tab_helper->ipc_router().OnMessageReceived(*message);

  EXPECT_CALL(*mock_delegate(), OnUndoMostVisitedDeletion(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoMostVisitedDeletion()).Times(0);
  message.reset(new ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(
      contents->GetRoutingID(), invalid_page_id, item_url));
  search_tab_helper->ipc_router().OnMessageReceived(*message);

  EXPECT_CALL(*mock_delegate(), OnUndoAllMostVisitedDeletions()).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoAllMostVisitedDeletions()).Times(0);
  message.reset(new ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
      contents->GetRoutingID(), invalid_page_id));
  search_tab_helper->ipc_router().OnMessageReceived(*message);

  EXPECT_CALL(*mock_delegate(), FocusOmnibox(OMNIBOX_FOCUS_VISIBLE)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessFocusOmnibox(
      search_tab_helper->ipc_router().is_active_tab_)).Times(0);
  message.reset(new ChromeViewHostMsg_FocusOmnibox(
      contents->GetRoutingID(), invalid_page_id, OMNIBOX_FOCUS_VISIBLE));
  search_tab_helper->ipc_router().OnMessageReceived(*message);

  EXPECT_CALL(*mock_delegate(), OnLogEvent(NTP_MOUSEOVER)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(0);
  message.reset(new ChromeViewHostMsg_LogEvent(contents->GetRoutingID(),
                                               invalid_page_id, NTP_MOUSEOVER));
  search_tab_helper->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, SendSetPromoInformationMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*policy, ShouldSendSetPromoInformation()).Times(1)
      .WillOnce(testing::Return(true));

  GetSearchTabHelper(contents)->ipc_router().SetPromoInformation(true);
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxPromoInformation::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendSetPromoInformationMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*policy, ShouldSendSetPromoInformation()).Times(1)
      .WillOnce(testing::Return(false));

  GetSearchTabHelper(contents)->ipc_router().SetPromoInformation(false);
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxPromoInformation::ID));
}

TEST_F(SearchIPCRouterTest, SendSetDisplayInstantResultsMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*policy, ShouldSendSetDisplayInstantResults()).Times(1)
      .WillOnce(testing::Return(true));

  GetSearchTabHelper(contents)->ipc_router().SetDisplayInstantResults();
  EXPECT_TRUE(MessageWasSent(
      ChromeViewMsg_SearchBoxSetDisplayInstantResults::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendSetDisplayInstantResultsMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*policy, ShouldSendSetDisplayInstantResults()).Times(1)
      .WillOnce(testing::Return(false));

  GetSearchTabHelper(contents)->ipc_router().SetDisplayInstantResults();
  EXPECT_FALSE(MessageWasSent(
      ChromeViewMsg_SearchBoxSetDisplayInstantResults::ID));
}

TEST_F(SearchIPCRouterTest, SendSetSuggestionToPrefetch) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*policy, ShouldSendSetSuggestionToPrefetch()).Times(1)
      .WillOnce(testing::Return(true));

  GetSearchTabHelper(contents)->SetSuggestionToPrefetch(
      InstantSuggestion());
  EXPECT_TRUE(MessageWasSent(
      ChromeViewMsg_SearchBoxSetSuggestionToPrefetch::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendSetSuggestionToPrefetch) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*policy, ShouldSendSetSuggestionToPrefetch()).Times(1)
      .WillOnce(testing::Return(false));

  GetSearchTabHelper(contents)->SetSuggestionToPrefetch(
      InstantSuggestion());
  EXPECT_FALSE(MessageWasSent(
      ChromeViewMsg_SearchBoxSetSuggestionToPrefetch::ID));
}

TEST_F(SearchIPCRouterTest, SendMostVisitedItemsMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*policy, ShouldSendMostVisitedItems()).Times(1)
      .WillOnce(testing::Return(true));

  GetSearchTabHelper(contents)->ipc_router().SendMostVisitedItems(
      std::vector<InstantMostVisitedItem>());
  EXPECT_TRUE(MessageWasSent(
      ChromeViewMsg_SearchBoxMostVisitedItemsChanged::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendMostVisitedItemsMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*policy, ShouldSendMostVisitedItems()).Times(1)
      .WillOnce(testing::Return(false));

  GetSearchTabHelper(contents)->ipc_router().SendMostVisitedItems(
      std::vector<InstantMostVisitedItem>());
  EXPECT_FALSE(MessageWasSent(
      ChromeViewMsg_SearchBoxMostVisitedItemsChanged::ID));
}

TEST_F(SearchIPCRouterTest, SendThemeBackgroundInfoMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*policy, ShouldSendThemeBackgroundInfo()).Times(1)
      .WillOnce(testing::Return(true));

  GetSearchTabHelper(contents)->ipc_router().SendThemeBackgroundInfo(
      ThemeBackgroundInfo());
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxThemeChanged::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendThemeBackgroundInfoMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*policy, ShouldSendThemeBackgroundInfo()).Times(1)
      .WillOnce(testing::Return(false));

  GetSearchTabHelper(contents)->ipc_router().SendThemeBackgroundInfo(
      ThemeBackgroundInfo());
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxThemeChanged::ID));
}

TEST_F(SearchIPCRouterTest, SendSubmitMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*policy, ShouldSubmitQuery()).Times(1)
      .WillOnce(testing::Return(true));

  GetSearchTabHelper(contents)->ipc_router().Submit(string16());
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxSubmit::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendSubmitMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();

  content::WebContents* contents = web_contents();
  SetupMockDelegateAndPolicy(contents);
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(contents);
  EXPECT_CALL(*policy, ShouldSubmitQuery()).Times(1)
      .WillOnce(testing::Return(false));

  GetSearchTabHelper(contents)->ipc_router().Submit(string16());
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxSubmit::ID));
}
