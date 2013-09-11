// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
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
};

class MockSearchIPCRouterPolicy : public SearchIPCRouter::Policy {
 public:
  virtual ~MockSearchIPCRouterPolicy() {}

  MOCK_METHOD0(ShouldProcessSetVoiceSearchSupport, bool());
  MOCK_METHOD0(ShouldSendSetDisplayInstantResults, bool());
};

}  // namespace

class SearchIPCRouterTest : public ChromeRenderViewHostTestHarness {
 public:
  virtual void SetUp() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableInstantExtendedAPI);
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

TEST_F(SearchIPCRouterTest, SendSetDisplayInstantResultsMsg) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();

  SetupMockDelegateAndPolicy(web_contents());
  MockSearchIPCRouterPolicy* policy =
      GetSearchIPCRouterPolicy(web_contents());
  EXPECT_CALL(*policy, ShouldSendSetDisplayInstantResults()).Times(1)
      .WillOnce(testing::Return(true));

  GetSearchTabHelper(web_contents())->ipc_router().SetDisplayInstantResults();
  ASSERT_TRUE(MessageWasSent(
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
  ASSERT_FALSE(MessageWasSent(
      ChromeViewMsg_SearchBoxSetDisplayInstantResults::ID));
}
