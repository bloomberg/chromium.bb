// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/search/search_ipc_router.h"
#include "chrome/common/chrome_switches.h"
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

}  // namespace

class SearchTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  virtual void SetUp() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableInstantExtendedAPI);
    ChromeRenderViewHostTestHarness::SetUp();
    SearchTabHelper::CreateForWebContents(web_contents());
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
