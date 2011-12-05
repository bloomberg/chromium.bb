// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/offline/offline_load_page.h"
#include "chrome/browser/renderer_host/offline_resource_handler.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "content/common/view_messages.h"
#include "content/test/test_browser_thread.h"

using content::BrowserThread;

static const char* kURL1 = "http://www.google.com/";
static const char* kURL2 = "http://www.gmail.com/";

namespace chromeos {

class OfflineLoadPageTest;

namespace {

// An OfflineLoadPage class that does not create windows.
class TestOfflineLoadPage :  public chromeos::OfflineLoadPage {
 public:
  TestOfflineLoadPage(TabContents* tab_contents,
                      const GURL& url,
                      OfflineLoadPageTest* test_page)
    : chromeos::OfflineLoadPage(tab_contents, url, NULL),
      test_page_(test_page) {
  }

  // Overriden from InterstitialPage.  Don't create a view.
  virtual TabContentsView* CreateTabContentsView() OVERRIDE {
    return NULL;
  }

  // chromeos::OfflineLoadPage override.
  virtual void NotifyBlockingPageComplete(bool proceed) OVERRIDE;

 private:
  OfflineLoadPageTest* test_page_;

  DISALLOW_COPY_AND_ASSIGN(TestOfflineLoadPage);
};

}  // namespace

class OfflineLoadPageTest : public ChromeRenderViewHostTestHarness {
 public:
  // The decision the user made.
  enum UserResponse {
    PENDING,
    OK,
    CANCEL
  };

  OfflineLoadPageTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()),
        io_thread_(BrowserThread::IO, MessageLoop::current()) {
  }

  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();
    user_response_ = PENDING;
  }

  void OnBlockingPageComplete(bool proceed) {
    if (proceed)
      user_response_ = OK;
    else
      user_response_ = CANCEL;
  }

  void Navigate(const char* url, int page_id) {
    ViewHostMsg_FrameNavigate_Params params;
    InitNavigateParams(
        &params, page_id, GURL(url), content::PAGE_TRANSITION_TYPED);
    contents()->TestDidNavigate(contents()->render_view_host(), params);
  }

  void ShowInterstitial(const char* url) {
    (new TestOfflineLoadPage(contents(), GURL(url), this))->Show();
  }

  // Returns the OfflineLoadPage currently showing or NULL if none is
  // showing.
  InterstitialPage* GetOfflineLoadPage() {
    return InterstitialPage::GetInterstitialPage(contents());
  }

  UserResponse user_response() const { return user_response_; }

 private:
  UserResponse user_response_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;

  // Initializes / shuts down a stub CrosLibrary.
  chromeos::ScopedStubCrosEnabler stub_cros_enabler_;

  DISALLOW_COPY_AND_ASSIGN(OfflineLoadPageTest);
};

void TestOfflineLoadPage::NotifyBlockingPageComplete(bool proceed) {
  test_page_->OnBlockingPageComplete(proceed);
}


TEST_F(OfflineLoadPageTest, OfflinePageProceed) {
  // Start a load.
  Navigate(kURL1, 1);
  // Load next page.
  controller().LoadURL(GURL(kURL2), content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing an offline browsing interstitial page
  // to be shown.
  ShowInterstitial(kURL2);
  InterstitialPage* interstitial = GetOfflineLoadPage();
  ASSERT_TRUE(interstitial);
  MessageLoop::current()->RunAllPending();

  // Simulate the user clicking "proceed".
  interstitial->Proceed();
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(OK, user_response());

  // The URL remains to be URL2.
  EXPECT_EQ(kURL2, contents()->GetURL().spec());

  // Commit navigation and the interstitial page is gone.
  Navigate(kURL2, 2);
  EXPECT_FALSE(GetOfflineLoadPage());
}

// Tests showing an offline page and not proceeding.
TEST_F(OfflineLoadPageTest, OfflinePageDontProceed) {
  // Start a load.
  Navigate(kURL1, 1);
  controller().LoadURL(GURL(kURL2), content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing an offline interstitial page to be shown.
  ShowInterstitial(kURL2);
  InterstitialPage* interstitial = GetOfflineLoadPage();
  ASSERT_TRUE(interstitial);
  MessageLoop::current()->RunAllPending();

  // Simulate the user clicking "don't proceed".
  interstitial->DontProceed();

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetOfflineLoadPage());
  // We did not proceed, the pending entry should be gone.
  EXPECT_FALSE(controller().pending_entry());
  // the URL is set back to kURL1.
  EXPECT_EQ(kURL1, contents()->GetURL().spec());
}

}  // namespace chromeos
