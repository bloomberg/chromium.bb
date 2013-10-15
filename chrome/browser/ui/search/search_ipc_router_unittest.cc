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
#include "chrome/common/chrome_switches.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/ntp_logging_events.h"
#include "chrome/common/omnibox_focus_state.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
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
  MOCK_METHOD0(ShouldProcessFocusOmnibox, bool());
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

class SearchIPCRouterTest : public ChromeRenderViewHostTestHarness {
 public:
  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();
    SearchTabHelper::CreateForWebContents(web_contents());
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
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*mock_delegate(), OnSetVoiceSearchSupport(true)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessSetVoiceSearchSupport()).Times(1)
      .WillOnce(testing::Return(true));

  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SetVoiceSearchSupported(
          web_contents()->GetRoutingID(),
          web_contents()->GetController().GetVisibleEntry()->GetPageID(),
          true));
  GetSearchTabHelper(web_contents())->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreVoiceSearchSupportMsg) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();
  EXPECT_CALL(*mock_delegate(), OnSetVoiceSearchSupport(true)).Times(0);

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*policy, ShouldProcessSetVoiceSearchSupport()).Times(1)
      .WillOnce(testing::Return(false));

  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SetVoiceSearchSupported(
          web_contents()->GetRoutingID(),
          web_contents()->GetController().GetVisibleEntry()->GetPageID(),
          true));
  GetSearchTabHelper(web_contents())->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessFocusOmniboxMsg) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*mock_delegate(), FocusOmnibox(OMNIBOX_FOCUS_VISIBLE)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessFocusOmnibox()).Times(1)
      .WillOnce(testing::Return(true));

  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_FocusOmnibox(
      web_contents()->GetRoutingID(),
      web_contents()->GetController().GetVisibleEntry()->GetPageID(),
      OMNIBOX_FOCUS_VISIBLE));
  GetSearchTabHelper(web_contents())->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreFocusOmniboxMsg) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*mock_delegate(), FocusOmnibox(OMNIBOX_FOCUS_VISIBLE)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessFocusOmnibox()).Times(1)
      .WillOnce(testing::Return(false));

  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_FocusOmnibox(
      web_contents()->GetRoutingID(),
      web_contents()->GetController().GetVisibleEntry()->GetPageID(),
      OMNIBOX_FOCUS_VISIBLE));
  GetSearchTabHelper(web_contents())->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessLogEventMsg) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();
  EXPECT_CALL(*mock_delegate(), OnLogEvent(NTP_MOUSEOVER)).Times(1);

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());

  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(1)
      .WillOnce(testing::Return(true));

  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_LogEvent(
      web_contents()->GetRoutingID(),
      web_contents()->GetController().GetVisibleEntry()->GetPageID(),
      NTP_MOUSEOVER));
  GetSearchTabHelper(web_contents())->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreLogEventMsg) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();
  EXPECT_CALL(*mock_delegate(), OnLogEvent(NTP_MOUSEOVER)).Times(0);

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(1)
      .WillOnce(testing::Return(false));

  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_LogEvent(
      web_contents()->GetRoutingID(),
      web_contents()->GetController().GetVisibleEntry()->GetPageID(),
      NTP_MOUSEOVER));
  GetSearchTabHelper(web_contents())->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessDeleteMostVisitedItemMsg) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());

  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnDeleteMostVisitedItem(item_url)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessDeleteMostVisitedItem()).Times(1)
      .WillOnce(testing::Return(true));

  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(
          web_contents()->GetRoutingID(),
          web_contents()->GetController().GetVisibleEntry()->GetPageID(),
          item_url));
  GetSearchTabHelper(web_contents())->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreDeleteMostVisitedItemMsg) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());

  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnDeleteMostVisitedItem(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessDeleteMostVisitedItem()).Times(1)
      .WillOnce(testing::Return(false));

  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(
          web_contents()->GetRoutingID(),
          web_contents()->GetController().GetVisibleEntry()->GetPageID(),
          item_url));
  GetSearchTabHelper(web_contents())->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessUndoMostVisitedDeletionMsg) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnUndoMostVisitedDeletion(item_url)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessUndoMostVisitedDeletion()).Times(1)
      .WillOnce(testing::Return(true));

  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(
          web_contents()->GetRoutingID(),
          web_contents()->GetController().GetVisibleEntry()->GetPageID(),
          item_url));
  GetSearchTabHelper(web_contents())->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreUndoMostVisitedDeletionMsg) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnUndoMostVisitedDeletion(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoMostVisitedDeletion()).Times(1)
      .WillOnce(testing::Return(false));

  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(
          web_contents()->GetRoutingID(),
          web_contents()->GetController().GetVisibleEntry()->GetPageID(),
          item_url));
  GetSearchTabHelper(web_contents())->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessUndoAllMostVisitedDeletionsMsg) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*mock_delegate(), OnUndoAllMostVisitedDeletions()).Times(1);
  EXPECT_CALL(*policy, ShouldProcessUndoAllMostVisitedDeletions()).Times(1)
      .WillOnce(testing::Return(true));

  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
          web_contents()->GetRoutingID(),
          web_contents()->GetController().GetVisibleEntry()->GetPageID()));
  GetSearchTabHelper(web_contents())->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreUndoAllMostVisitedDeletionsMsg) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*mock_delegate(), OnUndoAllMostVisitedDeletions()).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoAllMostVisitedDeletions()).Times(1)
      .WillOnce(testing::Return(false));

  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
          web_contents()->GetRoutingID(),
          web_contents()->GetController().GetVisibleEntry()->GetPageID()));
  GetSearchTabHelper(web_contents())->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreMessageIfThePageIsNotActive) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());

  int invalid_page_id = 1000;
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnDeleteMostVisitedItem(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessDeleteMostVisitedItem()).Times(0);
  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(
          web_contents()->GetRoutingID(), invalid_page_id, item_url));
  GetSearchTabHelper(web_contents())->ipc_router().OnMessageReceived(*message);

  EXPECT_CALL(*mock_delegate(), OnUndoMostVisitedDeletion(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoMostVisitedDeletion()).Times(0);
  message.reset(new ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(
      web_contents()->GetRoutingID(), invalid_page_id, item_url));
  GetSearchTabHelper(web_contents())->ipc_router().OnMessageReceived(*message);

  EXPECT_CALL(*mock_delegate(), OnUndoAllMostVisitedDeletions()).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoAllMostVisitedDeletions()).Times(0);
  message.reset(new ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
      web_contents()->GetRoutingID(), invalid_page_id));
  GetSearchTabHelper(web_contents())->ipc_router().OnMessageReceived(*message);

  EXPECT_CALL(*mock_delegate(), FocusOmnibox(OMNIBOX_FOCUS_VISIBLE)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessFocusOmnibox()).Times(0);
  message.reset(new ChromeViewHostMsg_FocusOmnibox(
      web_contents()->GetRoutingID(), invalid_page_id, OMNIBOX_FOCUS_VISIBLE));

  EXPECT_CALL(*mock_delegate(), OnLogEvent(NTP_MOUSEOVER)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(0);
  message.reset(new ChromeViewHostMsg_LogEvent(web_contents()->GetRoutingID(),
                                               invalid_page_id, NTP_MOUSEOVER));
  GetSearchTabHelper(web_contents())->ipc_router().OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, SendSetPromoInformationMsg) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*policy, ShouldSendSetPromoInformation()).Times(1)
      .WillOnce(testing::Return(true));

  GetSearchTabHelper(web_contents())->ipc_router().SetPromoInformation(true);
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxPromoInformation::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendSetPromoInformationMsg) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*policy, ShouldSendSetPromoInformation()).Times(1)
      .WillOnce(testing::Return(false));

  GetSearchTabHelper(web_contents())->ipc_router().SetPromoInformation(false);
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxPromoInformation::ID));
}

TEST_F(SearchIPCRouterTest, SendSetDisplayInstantResultsMsg) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*policy, ShouldSendSetDisplayInstantResults()).Times(1)
      .WillOnce(testing::Return(true));

  GetSearchTabHelper(web_contents())->ipc_router().SetDisplayInstantResults();
  EXPECT_TRUE(MessageWasSent(
      ChromeViewMsg_SearchBoxSetDisplayInstantResults::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendSetDisplayInstantResultsMsg) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*policy, ShouldSendSetDisplayInstantResults()).Times(1)
      .WillOnce(testing::Return(false));

  GetSearchTabHelper(web_contents())->ipc_router().SetDisplayInstantResults();
  EXPECT_FALSE(MessageWasSent(
      ChromeViewMsg_SearchBoxSetDisplayInstantResults::ID));
}

TEST_F(SearchIPCRouterTest, SendSetSuggestionToPrefetch) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*policy, ShouldSendSetSuggestionToPrefetch()).Times(1)
      .WillOnce(testing::Return(true));

  GetSearchTabHelper(web_contents())->SetSuggestionToPrefetch(
      InstantSuggestion());
  EXPECT_TRUE(MessageWasSent(
      ChromeViewMsg_SearchBoxSetSuggestionToPrefetch::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendSetSuggestionToPrefetch) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*policy, ShouldSendSetSuggestionToPrefetch()).Times(1)
      .WillOnce(testing::Return(false));

  GetSearchTabHelper(web_contents())->SetSuggestionToPrefetch(
      InstantSuggestion());
  EXPECT_FALSE(MessageWasSent(
      ChromeViewMsg_SearchBoxSetSuggestionToPrefetch::ID));
}

TEST_F(SearchIPCRouterTest, SendMostVisitedItemsMsg) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*policy, ShouldSendMostVisitedItems()).Times(1)
      .WillOnce(testing::Return(true));

  GetSearchTabHelper(web_contents())->ipc_router().SendMostVisitedItems(
      std::vector<InstantMostVisitedItem>());
  EXPECT_TRUE(MessageWasSent(
      ChromeViewMsg_SearchBoxMostVisitedItemsChanged::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendMostVisitedItemsMsg) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*policy, ShouldSendMostVisitedItems()).Times(1)
      .WillOnce(testing::Return(false));

  GetSearchTabHelper(web_contents())->ipc_router().SendMostVisitedItems(
      std::vector<InstantMostVisitedItem>());
  EXPECT_FALSE(MessageWasSent(
      ChromeViewMsg_SearchBoxMostVisitedItemsChanged::ID));
}

TEST_F(SearchIPCRouterTest, SendThemeBackgroundInfoMsg) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*policy, ShouldSendThemeBackgroundInfo()).Times(1)
      .WillOnce(testing::Return(true));

  GetSearchTabHelper(web_contents())->ipc_router().SendThemeBackgroundInfo(
      ThemeBackgroundInfo());
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxThemeChanged::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendThemeBackgroundInfoMsg) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*policy, ShouldSendThemeBackgroundInfo()).Times(1)
      .WillOnce(testing::Return(false));

  GetSearchTabHelper(web_contents())->ipc_router().SendThemeBackgroundInfo(
      ThemeBackgroundInfo());
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxThemeChanged::ID));
}

TEST_F(SearchIPCRouterTest, SendSubmitMsg) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*policy, ShouldSubmitQuery()).Times(1)
      .WillOnce(testing::Return(true));

  GetSearchTabHelper(web_contents())->ipc_router().Submit(string16());
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxSubmit::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendSubmitMsg) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*policy, ShouldSubmitQuery()).Times(1)
      .WillOnce(testing::Return(false));

  GetSearchTabHelper(web_contents())->ipc_router().Submit(string16());
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxSubmit::ID));
}
