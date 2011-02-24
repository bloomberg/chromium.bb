// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/offline/offline_load_page.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/test_tab_contents.h"

static const char* kURL1 = "http://www.google.com/";
static const char* kURL2 = "http://www.gmail.com/";

namespace {

// An OfflineLoadPage class that does not create windows.
class TestOfflineLoadPage :  public chromeos::OfflineLoadPage {
 public:
  TestOfflineLoadPage(TabContents* tab_contents,
                      const GURL& url,
                      Delegate* delegate)
    : chromeos::OfflineLoadPage(tab_contents, url, delegate) {
    EnableTest();
  }

  // Overriden from InterstitialPage.  Don't create a view.
  virtual TabContentsView* CreateTabContentsView() {
    return NULL;
  }
};

}  // namespace

namespace chromeos {

class OfflineLoadPageTest : public RenderViewHostTestHarness,
                            public OfflineLoadPage::Delegate {
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
    RenderViewHostTestHarness::SetUp();
    user_response_ = PENDING;
  }

  // OfflineLoadPage::Delegate implementation.
  virtual void OnBlockingPageComplete(bool proceed) {
    if (proceed)
      user_response_ = OK;
    else
      user_response_ = CANCEL;
  }

  void Navigate(const char* url, int page_id) {
    ViewHostMsg_FrameNavigate_Params params;
    InitNavigateParams(&params, page_id, GURL(url), PageTransition::TYPED);
    contents()->TestDidNavigate(contents_->render_view_host(), params);
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
  BrowserThread ui_thread_;
  BrowserThread io_thread_;
};


TEST_F(OfflineLoadPageTest, OfflinePaeProceed) {
  // Start a load.
  Navigate(kURL1, 1);
  // Load next page.
  controller().LoadURL(GURL(kURL2), GURL(), PageTransition::TYPED);

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
  controller().LoadURL(GURL(kURL2), GURL(), PageTransition::TYPED);

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
