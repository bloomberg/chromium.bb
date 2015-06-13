// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_debug_blocking_page.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_debug_ui_manager.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"

static const char* kBypassURL = "http://www.bypass.com/";
static const char* kBypassURL2 = "http://www.bypass2.com/";
static const char* kBypassURL3 = "http://www.bypass3.com/";
static const char* kGoogleURL = "http://www.google.com/";
static const char* kOtherURL = "http://www.other.com/";

namespace data_reduction_proxy {

// A DataReductionProxyDebugBlockingPage class that does not create windows.
class TestDataReductionProxyDebugBlockingPage
    : public DataReductionProxyDebugBlockingPage {
 public:
  TestDataReductionProxyDebugBlockingPage(
      DataReductionProxyDebugUIManager* ui_manager,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      content::WebContents* web_contents,
      const BypassResourceList& bypass_resources,
      const std::string& app_locale)
      : DataReductionProxyDebugBlockingPage(
          ui_manager, io_task_runner, web_contents,
          bypass_resources, app_locale) {
    DontCreateViewForTesting();
  }
};

class TestDataReductionProxyDebugBlockingPageFactory
    : public DataReductionProxyDebugBlockingPageFactory {
 public:
  TestDataReductionProxyDebugBlockingPageFactory() {
  }

  ~TestDataReductionProxyDebugBlockingPageFactory() override {
  }

  DataReductionProxyDebugBlockingPage* CreateDataReductionProxyPage(
      DataReductionProxyDebugUIManager* ui_manager,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      content::WebContents* web_contents,
      const DataReductionProxyDebugBlockingPage::BypassResourceList&
          resource_list,
          const std::string& app_locale) override {
    return new TestDataReductionProxyDebugBlockingPage(
        ui_manager, io_task_runner, web_contents, resource_list, app_locale);
  }
};

// TODO(megjablon): Add tests that verify the reception of a bypass message
// triggers the interstitial.
class DataReductionProxyDebugBlockingPageTest
    : public content::RenderViewHostTestHarness {
 public:
  // The decision the user made.
  enum UserResponse {
    PENDING,
    OK,
    CANCEL
  };

  DataReductionProxyDebugBlockingPageTest() : user_response_(PENDING) {
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    DataReductionProxyDebugBlockingPage::RegisterFactory(&factory_);
    ui_manager_ = new DataReductionProxyDebugUIManager(
        base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get(), "en-US");
    ResetUserResponse();
  }

  void TearDown() override {
    // Release the UI manager before the BrowserThreads are destroyed.
    ui_manager_ = NULL;
    DataReductionProxyDebugBlockingPage::RegisterFactory(NULL);
    content::RenderViewHostTestHarness::TearDown();
  }

  void OnBlockingPageComplete(bool proceed) {
    if (proceed)
      user_response_ = OK;
    else
      user_response_ = CANCEL;
  }

  void Navigate(const char* url,
                int page_id,
                int nav_entry_id,
                bool did_create_new_entry) {
    NavigateInternal(url, page_id, nav_entry_id, did_create_new_entry, false);
  }

  void NavigateCrossSite(const char* url,
                         int page_id,
                         int nav_entry_id,
                         bool did_create_new_entry) {
    NavigateInternal(url, page_id, nav_entry_id, did_create_new_entry, true);
  }

  void NavigateInternal(const char* url,
                        int page_id,
                        int nav_entry_id,
                        bool did_create_new_entry,
                        bool is_cross_site) {
    // The pending RVH should commit for cross-site navigations.
    content::RenderFrameHost* render_frame_host =
        is_cross_site
            ? content::WebContentsTester::For(web_contents())
                  ->GetPendingMainFrame()
            : web_contents()->GetMainFrame();

    content::WebContentsTester::For(web_contents())
        ->TestDidNavigate(render_frame_host, page_id, nav_entry_id,
                          did_create_new_entry, GURL(url),
                          ui::PAGE_TRANSITION_TYPED);
  }

  void GoBack(bool is_cross_site) {
    content::NavigationEntry* entry =
        web_contents()->GetController().GetEntryAtOffset(-1);
    ASSERT_TRUE(entry);
    web_contents()->GetController().GoBack();

    // The pending RVH should commit for cross-site navigations.
    content::RenderFrameHost* render_frame_host =
        is_cross_site
            ? content::WebContentsTester::For(web_contents())
                  ->GetPendingMainFrame()
            : web_contents()->GetMainFrame();

    content::WebContentsTester::For(web_contents())
        ->TestDidNavigate(render_frame_host, entry->GetPageID(),
                          entry->GetUniqueID(), false, GURL(entry->GetURL()),
                          ui::PAGE_TRANSITION_TYPED);
  }

  void ShowInterstitial(bool is_subresource, const char* url) {
    DataReductionProxyDebugUIManager::BypassResource resource;
    InitResource(&resource, is_subresource, GURL(url));
    DataReductionProxyDebugBlockingPage::ShowBlockingPage(
        ui_manager_.get(), base::ThreadTaskRunnerHandle::Get(), resource,
        std::string());
  }

  // Returns the DataReductionProxyDebugBlockingPage currently showing or NULL
  // if none is showing.
  DataReductionProxyDebugBlockingPage*
  GetDataReductionProxyDebugBlockingPage() {
    content::InterstitialPage* interstitial =
        content::InterstitialPage::GetInterstitialPage(web_contents());
    if (!interstitial)
      return NULL;
    EXPECT_EQ(DataReductionProxyDebugBlockingPage::kTypeForTesting,
              interstitial->GetDelegateForTesting()->GetTypeForTesting());
    return static_cast<DataReductionProxyDebugBlockingPage*>(
        interstitial->GetDelegateForTesting());
  }

  UserResponse user_response() const {
    return user_response_;
  }

  void ResetUserResponse() {
    user_response_ = PENDING;
  }

  static void ProceedThroughInterstitial(
      DataReductionProxyDebugBlockingPage* interstitial) {
    interstitial->interstitial_page()->Proceed();
    base::RunLoop().RunUntilIdle();
  }

  static void DontProceedThroughInterstitial(
      DataReductionProxyDebugBlockingPage* interstitial) {
    interstitial->interstitial_page()->DontProceed();
    base::RunLoop().RunUntilIdle();
  }

  void DontProceedThroughSubresourceInterstitial(
      DataReductionProxyDebugBlockingPage* sb_interstitial) {
    // CommandReceived(kTakeMeBackCommand) does a back navigation for
    // subresource interstitials.
    GoBack(false);
    base::RunLoop().RunUntilIdle();
  }

 private:
  void InitResource(DataReductionProxyDebugUIManager::BypassResource* resource,
                    bool is_subresource,
                    const GURL& url) {
    resource->callback =
        base::Bind(&DataReductionProxyDebugBlockingPageTest::
                       OnBlockingPageComplete,
                   base::Unretained(this));
    resource->url = url;
    resource->is_subresource = is_subresource;
    resource->render_process_host_id =
        web_contents()->GetRenderProcessHost()->GetID();
    resource->render_view_id =
        web_contents()->GetRenderViewHost()->GetRoutingID();
  }

  scoped_refptr<DataReductionProxyDebugUIManager> ui_manager_;
  UserResponse user_response_;
  TestDataReductionProxyDebugBlockingPageFactory factory_;
};

// Tests showing a blocking page for a bypassed request and not proceeding.
TEST_F(DataReductionProxyDebugBlockingPageTest, BypassPageDontProceed) {
  // Start a load.
  controller().LoadURL(GURL(kBypassURL), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing an interstitial to be shown.
  ShowInterstitial(false, kBypassURL);
  DataReductionProxyDebugBlockingPage* interstitial =
      GetDataReductionProxyDebugBlockingPage();
  ASSERT_TRUE(interstitial);

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(interstitial);

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetDataReductionProxyDebugBlockingPage());

  // The user did not proceed, the pending entry should be gone.
  EXPECT_FALSE(controller().GetPendingEntry());
}

// Tests showing a blocking page for a bypassed request and proceeding.
TEST_F(DataReductionProxyDebugBlockingPageTest, BypassPageProceed) {
  // Start a load.
  controller().LoadURL(GURL(kBypassURL), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());
  int pending_id = controller().GetPendingEntry()->GetUniqueID();

  // Simulate the load causing an interstitial to be shown.
  ShowInterstitial(false, kBypassURL);
  DataReductionProxyDebugBlockingPage* interstitial =
      GetDataReductionProxyDebugBlockingPage();
  ASSERT_TRUE(interstitial);

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "proceed".
  ProceedThroughInterstitial(interstitial);

  // The interstitial is shown until the navigation commits.
  ASSERT_TRUE(GetDataReductionProxyDebugBlockingPage());
  // Commit the navigation.
  Navigate(kBypassURL, 1, pending_id, true);
  // The interstitial should be gone now.
  EXPECT_EQ(OK, user_response());
  ASSERT_FALSE(GetDataReductionProxyDebugBlockingPage());
}

// Tests showing a blocking page for a page that contains bypassed subresources
// and not proceeding.
TEST_F(DataReductionProxyDebugBlockingPageTest, BypassSubresourceDontProceed) {
  // Navigate somewhere.
  Navigate(kGoogleURL, 1, 0, true);

  // Navigate somewhere else.
  Navigate(kOtherURL, 2, 0, true);

  // Simulate that page loading a bypass-resource triggering an interstitial.
  ShowInterstitial(true, kBypassURL);

  DataReductionProxyDebugBlockingPage* interstitial =
      GetDataReductionProxyDebugBlockingPage();
  ASSERT_TRUE(interstitial);

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "don't proceed".
  DontProceedThroughSubresourceInterstitial(interstitial);
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetDataReductionProxyDebugBlockingPage());

  // The user did not proceed, the controler should be back to the first page,
  // the 2nd one should have been removed from the navigation controller.
  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_EQ(kGoogleURL, controller().GetLastCommittedEntry()->GetURL().spec());
}

// Tests showing a blocking page for a page that contains bypassed subresources
// and proceeding.
TEST_F(DataReductionProxyDebugBlockingPageTest, BypassSubresourceProceed) {
  // Navigate somewhere.
  Navigate(kGoogleURL, 1, 0, true);

  // Simulate that page loading a bypass-resource triggering an interstitial.
  ShowInterstitial(true, kBypassURL);

  DataReductionProxyDebugBlockingPage* interstitial =
      GetDataReductionProxyDebugBlockingPage();
  ASSERT_TRUE(interstitial);

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "proceed".
  ProceedThroughInterstitial(interstitial);
  EXPECT_EQ(OK, user_response());
  EXPECT_FALSE(GetDataReductionProxyDebugBlockingPage());

  // The user did proceed, the controller should be back to showing the page.
  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_EQ(kGoogleURL, controller().GetLastCommittedEntry()->GetURL().spec());
}

// Tests showing a blocking page for a page that contains multiple bypassed
// subresources and not proceeding. This just tests that the extra bypassed
// subresources (which trigger queued interstitial pages) do not break anything.
TEST_F(DataReductionProxyDebugBlockingPageTest,
       BypassMultipleSubresourcesDontProceed) {
  // Navigate somewhere.
  Navigate(kGoogleURL, 1, 0, true);

  // Navigate somewhere else.
  Navigate(kOtherURL, 2, 0, true);

  // Simulate that page loading a bypass-resource triggering an interstitial.
  ShowInterstitial(true, kBypassURL);

  // More bypassed resources loading causing more interstitials. The new
  // interstitials should be queued.
  ShowInterstitial(true, kBypassURL2);
  ShowInterstitial(true, kBypassURL3);

  DataReductionProxyDebugBlockingPage* interstitial =
      GetDataReductionProxyDebugBlockingPage();
  ASSERT_TRUE(interstitial);

  // Simulate the user clicking "don't proceed".
  DontProceedThroughSubresourceInterstitial(interstitial);
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetDataReductionProxyDebugBlockingPage());

  // The user did not proceed, the controller should be back to the first page,
  // the 2nd one should have been removed from the navigation controller.
  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_EQ(kGoogleURL, controller().GetLastCommittedEntry()->GetURL().spec());
}

// Tests showing a blocking page for a page that contains multiple bypassed
// subresources. Proceeding through the first interstitial should proceed
// through all interstitials.
TEST_F(DataReductionProxyDebugBlockingPageTest,
       BypassMultipleSubresourcesProceed) {
  // Navigate somewhere.
  Navigate(kGoogleURL, 1, 0, true);

  // Simulate that page loading a bypass-resource triggering an interstitial.
  ShowInterstitial(true, kBypassURL);

  // More bypassed resources loading causing more interstitials. The new
  // interstitials should be queued.
  ShowInterstitial(true, kBypassURL2);
  ShowInterstitial(true, kBypassURL3);

  DataReductionProxyDebugBlockingPage* interstitial =
      GetDataReductionProxyDebugBlockingPage();
  ASSERT_TRUE(interstitial);

  // Proceed through the 1st interstitial.
  ProceedThroughInterstitial(interstitial);
  EXPECT_EQ(OK, user_response());

  // The user did proceed, the controller should be back to showing the page.
  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_EQ(kGoogleURL, controller().GetLastCommittedEntry()->GetURL().spec());
}

// Tests showing a blocking page then navigating back and forth to make sure the
// controller entries are OK.
TEST_F(DataReductionProxyDebugBlockingPageTest, NavigatingBackAndForth) {
  // Navigate somewhere.
  Navigate(kGoogleURL, 1, 0, true);

  // Now navigate to a bypassed page triggerring an interstitial.
  controller().LoadURL(GURL(kBypassURL), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());
  int pending_id = controller().GetPendingEntry()->GetUniqueID();
  ShowInterstitial(false, kBypassURL);
  DataReductionProxyDebugBlockingPage* interstitial =
      GetDataReductionProxyDebugBlockingPage();
  ASSERT_TRUE(interstitial);

  // Proceed through the 1st interstitial.
  ProceedThroughInterstitial(interstitial);
  NavigateCrossSite(kBypassURL, 2, pending_id, true);  // Commit navigation.
  GoBack(true);

  // We are back on the first page.
  interstitial = GetDataReductionProxyDebugBlockingPage();
  ASSERT_FALSE(interstitial);
  ASSERT_EQ(2, controller().GetEntryCount());
  EXPECT_EQ(kGoogleURL, controller().GetLastCommittedEntry()->GetURL().spec());

  // Navigate forward to the bypassed URL.
  web_contents()->GetController().GoForward();
  pending_id = controller().GetPendingEntry()->GetUniqueID();
  ShowInterstitial(false, kBypassURL);
  interstitial = GetDataReductionProxyDebugBlockingPage();
  ASSERT_TRUE(interstitial);

  // Let's proceed and make sure everything is OK.
  ProceedThroughInterstitial(interstitial);
  // Commit the navigation.
  NavigateCrossSite(kBypassURL, 2, pending_id, false);
  interstitial = GetDataReductionProxyDebugBlockingPage();
  ASSERT_FALSE(interstitial);
  ASSERT_EQ(2, controller().GetEntryCount());
  EXPECT_EQ(kBypassURL, controller().GetLastCommittedEntry()->GetURL().spec());
}

// Tests that calling "don't proceed" after "proceed" has been called causes
// the interstitial to disappear.
TEST_F(DataReductionProxyDebugBlockingPageTest, ProceedThenDontProceed) {
  // Start a load.
  controller().LoadURL(GURL(kBypassURL), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing an interstitial to be shown.
  ShowInterstitial(false, kBypassURL);
  DataReductionProxyDebugBlockingPage* interstitial =
      GetDataReductionProxyDebugBlockingPage();
  ASSERT_TRUE(interstitial);

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "proceed" then "don't proceed".
  interstitial->interstitial_page()->Proceed();
  interstitial->interstitial_page()->DontProceed();
  base::RunLoop().RunUntilIdle();

  // The interstitial should be gone.
  EXPECT_EQ(OK, user_response());
  EXPECT_FALSE(GetDataReductionProxyDebugBlockingPage());
}

}  // namespace data_reduction_proxy
