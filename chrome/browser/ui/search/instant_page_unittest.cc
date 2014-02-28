// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_page.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
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

class FakePageDelegate : public InstantPage::Delegate {
 public:
  virtual ~FakePageDelegate() {
  }

  MOCK_METHOD2(InstantSupportDetermined,
               void(const content::WebContents* contents,
                    bool supports_instant));
  MOCK_METHOD1(InstantPageRenderProcessGone,
               void(const content::WebContents* contents));
  MOCK_METHOD2(InstantPageAboutToNavigateMainFrame,
               void(const content::WebContents* contents,
                    const GURL& url));
  MOCK_METHOD5(NavigateToURL,
               void(const content::WebContents* contents,
                    const GURL& url,
                    content::PageTransition transition,
                    WindowOpenDisposition disposition,
                    bool is_search_type));
};

}  // namespace

class InstantPageTest : public ChromeRenderViewHostTestHarness {
 public:
  virtual void SetUp() OVERRIDE;

  bool MessageWasSent(uint32 id) {
    return process()->sink().GetFirstMessageMatching(id) != NULL;
  }

  scoped_ptr<InstantPage> page;
  FakePageDelegate delegate;
};

void InstantPageTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  SearchTabHelper::CreateForWebContents(web_contents());
}

TEST_F(InstantPageTest, IsLocal) {
  page.reset(new InstantPage(&delegate, "", NULL, false));
  EXPECT_FALSE(page->supports_instant());
  EXPECT_FALSE(page->IsLocal());
  page->SetContents(web_contents());
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(page->IsLocal());
  NavigateAndCommit(GURL("http://example.com"));
  EXPECT_FALSE(page->IsLocal());
}

TEST_F(InstantPageTest, DetermineIfPageSupportsInstant_Local) {
  page.reset(new InstantPage(&delegate, "", NULL, false));
  EXPECT_FALSE(page->supports_instant());
  page->SetContents(web_contents());
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(page->IsLocal());
  EXPECT_CALL(delegate, InstantSupportDetermined(web_contents(), true))
      .Times(1);
  SearchTabHelper::FromWebContents(web_contents())->
      DetermineIfPageSupportsInstant();
  EXPECT_TRUE(page->supports_instant());
}

TEST_F(InstantPageTest, DetermineIfPageSupportsInstant_NonLocal) {
  page.reset(new InstantPage(&delegate, "", NULL, false));
  EXPECT_FALSE(page->supports_instant());
  page->SetContents(web_contents());
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(page->IsLocal());
  process()->sink().ClearMessages();
  SearchTabHelper::FromWebContents(web_contents())->
      DetermineIfPageSupportsInstant();
  const IPC::Message* message = process()->sink().GetFirstMessageMatching(
      ChromeViewMsg_DetermineIfPageSupportsInstant::ID);
  ASSERT_TRUE(message != NULL);
  EXPECT_EQ(web_contents()->GetRoutingID(), message->routing_id());
}

TEST_F(InstantPageTest, PageURLDoesntBelongToInstantRenderer) {
  page.reset(new InstantPage(&delegate, "", NULL, false));
  EXPECT_FALSE(page->supports_instant());
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  page->SetContents(web_contents());

  // Navigate to a page URL that doesn't belong to Instant renderer.
  // SearchTabHelper::DeterminerIfPageSupportsInstant() should return
  // immediately without dispatching any message to the renderer.
  NavigateAndCommit(GURL("http://www.example.com"));
  EXPECT_FALSE(page->IsLocal());
  process()->sink().ClearMessages();
  EXPECT_CALL(delegate, InstantSupportDetermined(web_contents(), false))
      .Times(1);

  SearchTabHelper::FromWebContents(web_contents())->
      DetermineIfPageSupportsInstant();
  const IPC::Message* message = process()->sink().GetFirstMessageMatching(
      ChromeViewMsg_DetermineIfPageSupportsInstant::ID);
  ASSERT_TRUE(message == NULL);
  EXPECT_FALSE(page->supports_instant());
}

// Test to verify that ChromeViewMsg_DetermineIfPageSupportsInstant message
// reply handler updates the instant support state in InstantPage.
TEST_F(InstantPageTest, PageSupportsInstant) {
  page.reset(new InstantPage(&delegate, "", NULL, false));
  EXPECT_FALSE(page->supports_instant());
  page->SetContents(web_contents());
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();
  SearchTabHelper::FromWebContents(web_contents())->
      DetermineIfPageSupportsInstant();
  const IPC::Message* message = process()->sink().GetFirstMessageMatching(
      ChromeViewMsg_DetermineIfPageSupportsInstant::ID);
  ASSERT_TRUE(message != NULL);
  EXPECT_EQ(web_contents()->GetRoutingID(), message->routing_id());

  EXPECT_CALL(delegate, InstantSupportDetermined(web_contents(), true))
      .Times(1);

  // Assume the page supports instant. Invoke the message reply handler to make
  // sure the InstantPage is notified about the instant support state.
  const content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  EXPECT_TRUE(entry);
  SearchTabHelper::FromWebContents(web_contents())->InstantSupportChanged(true);
  EXPECT_TRUE(page->supports_instant());
}
