// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/offline/offline_load_page.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"

using content::InterstitialPage;
using content::WebContents;
using content::WebContentsTester;

static const char* kURL1 = "http://www.google.com/";
static const char* kURL2 = "http://www.gmail.com/";

namespace chromeos {

class OfflineLoadPageTest;

// An OfflineLoadPage class that does not create windows.
class TestOfflineLoadPage :  public chromeos::OfflineLoadPage {
 public:
  TestOfflineLoadPage(WebContents* web_contents,
                      const GURL& url,
                      OfflineLoadPageTest* test_page)
    : chromeos::OfflineLoadPage(web_contents, url, CompletionCallback()),
      test_page_(test_page) {
    interstitial_page_->DontCreateViewForTesting();
  }

  // chromeos::OfflineLoadPage override.
  virtual void NotifyBlockingPageComplete(bool proceed) OVERRIDE;

 private:
  OfflineLoadPageTest* test_page_;

  DISALLOW_COPY_AND_ASSIGN(TestOfflineLoadPage);
};

class OfflineLoadPageTest : public ChromeRenderViewHostTestHarness {
 public:
  // The decision the user made.
  enum UserResponse {
    PENDING,
    OK,
    CANCEL
  };

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
    WebContentsTester::For(web_contents())->TestDidNavigate(
        web_contents()->GetMainFrame(), page_id, GURL(url),
        content::PAGE_TRANSITION_TYPED);
  }

  void ShowInterstitial(const char* url) {
    (new TestOfflineLoadPage(web_contents(), GURL(url), this))->Show();
  }

  // Returns the OfflineLoadPage currently showing or NULL if none is
  // showing.
  InterstitialPage* GetOfflineLoadPage() {
    return InterstitialPage::GetInterstitialPage(web_contents());
  }

  UserResponse user_response() const { return user_response_; }

 private:
  friend class TestOfflineLoadPage;

  UserResponse user_response_;
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
  base::MessageLoop::current()->RunUntilIdle();

  // Simulate the user clicking "proceed".
  interstitial->Proceed();
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(OK, user_response());

  // The URL remains to be URL2.
  EXPECT_EQ(kURL2, web_contents()->GetVisibleURL().spec());

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
  base::MessageLoop::current()->RunUntilIdle();

  // Simulate the user clicking "don't proceed".
  interstitial->DontProceed();

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetOfflineLoadPage());
  // We did not proceed, the pending entry should be gone.
  EXPECT_FALSE(controller().GetPendingEntry());
  // the URL is set back to kURL1.
  EXPECT_EQ(kURL1, web_contents()->GetLastCommittedURL().spec());
}

}  // namespace chromeos
