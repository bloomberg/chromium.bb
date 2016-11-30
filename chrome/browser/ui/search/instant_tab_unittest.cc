// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_tab.h"

#include <stdint.h>

#include <memory>

#include "base/command_line.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class Profile;

namespace {

class FakePageDelegate : public InstantTab::Delegate {
 public:
  virtual ~FakePageDelegate() {
  }

  MOCK_METHOD2(InstantSupportDetermined,
               void(const content::WebContents* contents,
                    bool supports_instant));
  MOCK_METHOD2(InstantTabAboutToNavigateMainFrame,
               void(const content::WebContents* contents,
                    const GURL& url));
};

}  // namespace

class InstantTabTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override;

  SearchTabHelper* search_tab() {
    return SearchTabHelper::FromWebContents(web_contents());
  }

  std::unique_ptr<InstantTab> page;
  FakePageDelegate delegate;
};

void InstantTabTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  SearchTabHelper::CreateForWebContents(web_contents());
}

TEST_F(InstantTabTest, DetermineIfPageSupportsInstant_Local) {
  page.reset(new InstantTab(&delegate, web_contents()));
  EXPECT_FALSE(search_tab()->SupportsInstant());
  page->Init();
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_CALL(delegate, InstantSupportDetermined(web_contents(), true));
  SearchTabHelper::FromWebContents(web_contents())->
      DetermineIfPageSupportsInstant();
  EXPECT_TRUE(search_tab()->SupportsInstant());
}

TEST_F(InstantTabTest, DetermineIfPageSupportsInstant_NonLocal) {
  page.reset(new InstantTab(&delegate, web_contents()));
  EXPECT_FALSE(search_tab()->SupportsInstant());
  page->Init();
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();
  SearchTabHelper::FromWebContents(web_contents())->
      DetermineIfPageSupportsInstant();
  const IPC::Message* message = process()->sink().GetFirstMessageMatching(
      ChromeViewMsg_DetermineIfPageSupportsInstant::ID);
  ASSERT_TRUE(message != NULL);
  EXPECT_EQ(web_contents()->GetRenderViewHost()->GetRoutingID(),
            message->routing_id());
}

TEST_F(InstantTabTest, PageURLDoesntBelongToInstantRenderer) {
  page.reset(new InstantTab(&delegate, web_contents()));
  EXPECT_FALSE(search_tab()->SupportsInstant());
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  page->Init();

  // Navigate to a page URL that doesn't belong to Instant renderer.
  // SearchTabHelper::DeterminerIfPageSupportsInstant() should return
  // immediately without dispatching any message to the renderer.
  NavigateAndCommit(GURL("http://www.example.com"));
  process()->sink().ClearMessages();
  EXPECT_CALL(delegate, InstantSupportDetermined(web_contents(), false));

  SearchTabHelper::FromWebContents(web_contents())->
      DetermineIfPageSupportsInstant();
  const IPC::Message* message = process()->sink().GetFirstMessageMatching(
      ChromeViewMsg_DetermineIfPageSupportsInstant::ID);
  ASSERT_TRUE(message == NULL);
  EXPECT_FALSE(search_tab()->SupportsInstant());
}

// Test to verify that ChromeViewMsg_DetermineIfPageSupportsInstant message
// reply handler updates the instant support state in InstantTab.
TEST_F(InstantTabTest, PageSupportsInstant) {
  page.reset(new InstantTab(&delegate, web_contents()));
  EXPECT_FALSE(search_tab()->SupportsInstant());
  page->Init();
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();
  SearchTabHelper::FromWebContents(web_contents())->
      DetermineIfPageSupportsInstant();
  const IPC::Message* message = process()->sink().GetFirstMessageMatching(
      ChromeViewMsg_DetermineIfPageSupportsInstant::ID);
  ASSERT_TRUE(message != NULL);
  EXPECT_EQ(web_contents()->GetRenderViewHost()->GetRoutingID(),
            message->routing_id());

  EXPECT_CALL(delegate, InstantSupportDetermined(web_contents(), true));

  // Assume the page supports instant. Invoke the message reply handler to make
  // sure the InstantTab is notified about the instant support state.
  const content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  EXPECT_TRUE(entry);
  SearchTabHelper::FromWebContents(web_contents())->InstantSupportChanged(true);
  EXPECT_TRUE(search_tab()->SupportsInstant());
}
