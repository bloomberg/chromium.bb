// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_tab.h"

#include <stdint.h>

#include <memory>

#include "base/command_line.h"
#include "chrome/browser/ui/search/search_ipc_router.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/search/mock_searchbox.h"
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

using testing::Return;

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
               void(const content::WebContents* contents, const GURL& url));
};

class MockSearchBoxClientFactory
    : public SearchIPCRouter::SearchBoxClientFactory {
 public:
  MOCK_METHOD0(GetSearchBox, chrome::mojom::SearchBox*(void));
};

}  // namespace

class InstantTabTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override;

  SearchTabHelper* search_tab() {
    return SearchTabHelper::FromWebContents(web_contents());
  }

  bool SupportsInstant() {
    return search_tab()->model()->instant_support() == INSTANT_SUPPORT_YES;
  }

  std::unique_ptr<InstantTab> page;
  FakePageDelegate delegate;
  MockSearchBox mock_search_box;
};

void InstantTabTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  SearchTabHelper::CreateForWebContents(web_contents());
  auto factory = base::MakeUnique<MockSearchBoxClientFactory>();
  ON_CALL(*factory, GetSearchBox()).WillByDefault(Return(&mock_search_box));
  search_tab()
      ->ipc_router_for_testing()
      .set_search_box_client_factory_for_testing(std::move(factory));
}

TEST_F(InstantTabTest, DetermineIfPageSupportsInstant_Local) {
  page.reset(new InstantTab(&delegate, web_contents()));
  EXPECT_FALSE(SupportsInstant());
  page->Init();
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_CALL(delegate, InstantSupportDetermined(web_contents(), true));
  search_tab()->DetermineIfPageSupportsInstant();
  EXPECT_TRUE(SupportsInstant());
}

TEST_F(InstantTabTest, DetermineIfPageSupportsInstant_NonLocal) {
  page.reset(new InstantTab(&delegate, web_contents()));
  EXPECT_FALSE(SupportsInstant());
  page->Init();
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  EXPECT_CALL(mock_search_box, DetermineIfPageSupportsInstant());
  search_tab()->DetermineIfPageSupportsInstant();
}

TEST_F(InstantTabTest, PageURLDoesntBelongToInstantRenderer) {
  page.reset(new InstantTab(&delegate, web_contents()));
  EXPECT_FALSE(SupportsInstant());
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  page->Init();

  // Navigate to a page URL that doesn't belong to Instant renderer.
  // SearchTabHelper::DeterminerIfPageSupportsInstant() should return
  // immediately without dispatching any message to the renderer.
  NavigateAndCommit(GURL("http://www.example.com"));
  EXPECT_CALL(delegate, InstantSupportDetermined(web_contents(), false));
  EXPECT_CALL(mock_search_box, DetermineIfPageSupportsInstant()).Times(0);

  search_tab()->DetermineIfPageSupportsInstant();
  EXPECT_FALSE(SupportsInstant());
}

// Test to verify that ChromeViewMsg_DetermineIfPageSupportsInstant message
// reply handler updates the instant support state in InstantTab.
TEST_F(InstantTabTest, PageSupportsInstant) {
  page.reset(new InstantTab(&delegate, web_contents()));
  EXPECT_FALSE(SupportsInstant());
  page->Init();
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  EXPECT_CALL(mock_search_box, DetermineIfPageSupportsInstant());
  search_tab()->DetermineIfPageSupportsInstant();

  EXPECT_CALL(delegate, InstantSupportDetermined(web_contents(), true));

  // Assume the page supports instant. Invoke the message reply handler to make
  // sure the InstantTab is notified about the instant support state.
  const content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  EXPECT_TRUE(entry);
  search_tab()->InstantSupportChanged(true);
  EXPECT_TRUE(SupportsInstant());
}
