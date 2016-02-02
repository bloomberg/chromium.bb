// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/threat_details.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"

using content::InterstitialPage;
using content::NavigationEntry;
using content::WebContents;
using content::WebContentsTester;

static const char* kGoogleURL = "http://www.google.com/";
static const char* kGoodURL = "http://www.goodguys.com/";
static const char* kGoodHTTPSURL = "https://www.goodguys.com/";
static const char* kBadURL = "http://www.badguys.com/";
static const char* kBadURL2 = "http://www.badguys2.com/";
static const char* kBadURL3 = "http://www.badguys3.com/";
static const char* kBadHTTPSURL = "https://www.badguys.com/";

namespace safe_browsing {

namespace {

// A SafeBrowingBlockingPage class that does not create windows.
class TestSafeBrowsingBlockingPage : public SafeBrowsingBlockingPage {
 public:
  TestSafeBrowsingBlockingPage(SafeBrowsingUIManager* manager,
                               WebContents* web_contents,
                               const GURL& main_frame_url,
                               const UnsafeResourceList& unsafe_resources)
      : SafeBrowsingBlockingPage(manager,
                                 web_contents,
                                 main_frame_url,
                                 unsafe_resources) {
    // Don't delay details at all for the unittest.
    threat_details_proceed_delay_ms_ = 0;
    DontCreateViewForTesting();
  }
};

class TestSafeBrowsingUIManager: public SafeBrowsingUIManager {
 public:
  explicit TestSafeBrowsingUIManager(SafeBrowsingService* service)
      : SafeBrowsingUIManager(service) {
  }

  void SendSerializedThreatDetails(const std::string& serialized) override {
    details_.push_back(serialized);
  }

  std::list<std::string>* GetDetails() {
    return &details_;
  }

 private:
  ~TestSafeBrowsingUIManager() override {}

  std::list<std::string> details_;
};

class TestSafeBrowsingBlockingPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  TestSafeBrowsingBlockingPageFactory() { }
  ~TestSafeBrowsingBlockingPageFactory() override {}

  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      SafeBrowsingUIManager* manager,
      WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      override {
    return new TestSafeBrowsingBlockingPage(manager, web_contents,
                                            main_frame_url, unsafe_resources);
  }
};

}  // namespace

class SafeBrowsingBlockingPageTest : public ChromeRenderViewHostTestHarness {
 public:
  // The decision the user made.
  enum UserResponse {
    PENDING,
    OK,
    CANCEL
  };

  SafeBrowsingBlockingPageTest() {
    ResetUserResponse();
    // The safe browsing UI manager does not need a service for this test.
    ui_manager_ = new TestSafeBrowsingUIManager(NULL);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SafeBrowsingBlockingPage::RegisterFactory(&factory_);
    ResetUserResponse();
  }

  void TearDown() override {
    // Release the UI manager before the BrowserThreads are destroyed.
    ui_manager_ = NULL;
    SafeBrowsingBlockingPage::RegisterFactory(NULL);
    // Clean up singleton reference (crbug.com/110594).
    ThreatDetails::RegisterFactory(NULL);
    ChromeRenderViewHostTestHarness::TearDown();
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
    NavigationEntry* entry =
        web_contents()->GetController().GetEntryAtOffset(-1);
    ASSERT_TRUE(entry);
    web_contents()->GetController().GoBack();

    // The pending RVH should commit for cross-site navigations.
    content::RenderFrameHost* rfh = is_cross_site ?
        WebContentsTester::For(web_contents())->GetPendingMainFrame() :
        web_contents()->GetMainFrame();
    WebContentsTester::For(web_contents())->TestDidNavigate(
        rfh,
        entry->GetPageID(),
        entry->GetUniqueID(),
        false,
        entry->GetURL(),
        ui::PAGE_TRANSITION_TYPED);
  }

  void ShowInterstitial(bool is_subresource, const char* url) {
    SafeBrowsingUIManager::UnsafeResource resource;
    InitResource(&resource, is_subresource, GURL(url));
    SafeBrowsingBlockingPage::ShowBlockingPage(ui_manager_.get(), resource);
  }

  // Returns the SafeBrowsingBlockingPage currently showing or NULL if none is
  // showing.
  SafeBrowsingBlockingPage* GetSafeBrowsingBlockingPage() {
    InterstitialPage* interstitial =
        InterstitialPage::GetInterstitialPage(web_contents());
    if (!interstitial)
      return NULL;
    return  static_cast<SafeBrowsingBlockingPage*>(
        interstitial->GetDelegateForTesting());
  }

  UserResponse user_response() const { return user_response_; }
  void ResetUserResponse() { user_response_ = PENDING; }

  static void ProceedThroughInterstitial(
      SafeBrowsingBlockingPage* sb_interstitial) {
    sb_interstitial->interstitial_page()->Proceed();
    // Proceed() posts a task to update the SafeBrowsingService::Client.
    base::RunLoop().RunUntilIdle();
  }

  static void DontProceedThroughInterstitial(
      SafeBrowsingBlockingPage* sb_interstitial) {
    sb_interstitial->interstitial_page()->DontProceed();
    // DontProceed() posts a task to update the SafeBrowsingService::Client.
    base::RunLoop().RunUntilIdle();
  }

  void DontProceedThroughSubresourceInterstitial(
      SafeBrowsingBlockingPage* sb_interstitial) {
    // CommandReceived(kTakeMeBackCommand) does a back navigation for
    // subresource interstitials.
    GoBack(false);
    // DontProceed() posts a task to update the SafeBrowsingService::Client.
    base::RunLoop().RunUntilIdle();
  }

  scoped_refptr<TestSafeBrowsingUIManager> ui_manager_;

 private:
  void InitResource(SafeBrowsingUIManager::UnsafeResource* resource,
                    bool is_subresource,
                    const GURL& url) {
    resource->callback =
        base::Bind(&SafeBrowsingBlockingPageTest::OnBlockingPageComplete,
                   base::Unretained(this));
    resource->callback_thread =
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::IO);
    resource->url = url;
    resource->is_subresource = is_subresource;
    resource->threat_type = SB_THREAT_TYPE_URL_MALWARE;
    resource->render_process_host_id =
        web_contents()->GetRenderProcessHost()->GetID();
    resource->render_frame_id = web_contents()->GetMainFrame()->GetRoutingID();
    resource->threat_source = safe_browsing::ThreatSource::LOCAL_PVER3;
  }

  UserResponse user_response_;
  TestSafeBrowsingBlockingPageFactory factory_;
};


// Tests showing a blocking page for a malware page and not proceeding.
TEST_F(SafeBrowsingBlockingPageTest, MalwarePageDontProceed) {
  // Enable malware details.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  // Start a load.
  controller().LoadURL(GURL(kBadURL), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());


  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(sb_interstitial);

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did not proceed, the pending entry should be gone.
  EXPECT_FALSE(controller().GetPendingEntry());

  // A report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();
}

// Tests showing a blocking page for a malware page and then proceeding.
TEST_F(SafeBrowsingBlockingPageTest, MalwarePageProceed) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  // Start a load.
  controller().LoadURL(GURL(kBadURL), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());
  int pending_id = controller().GetPendingEntry()->GetUniqueID();

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Simulate the user clicking "proceed".
  ProceedThroughInterstitial(sb_interstitial);

  // The interstitial is shown until the navigation commits.
  ASSERT_TRUE(InterstitialPage::GetInterstitialPage(web_contents()));
  // Commit the navigation.
  Navigate(kBadURL, 1, pending_id, true);
  // The interstitial should be gone now.
  ASSERT_FALSE(InterstitialPage::GetInterstitialPage(web_contents()));

  // A report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();
}

// Tests showing a blocking page for a page that contains malware subresources
// and not proceeding.
TEST_F(SafeBrowsingBlockingPageTest, PageWithMalwareResourceDontProceed) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  // Navigate somewhere.
  Navigate(kGoogleURL, 1, 0, true);

  // Navigate somewhere else.
  Navigate(kGoodURL, 2, 0, true);

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(true, kBadURL);

  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Simulate the user clicking "don't proceed".
  DontProceedThroughSubresourceInterstitial(sb_interstitial);
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did not proceed, we should be back to the first page, the 2nd one should
  // have been removed from the navigation controller.
  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_EQ(kGoogleURL, controller().GetActiveEntry()->GetURL().spec());

  // A report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();
}

// Tests showing a blocking page for a page that contains malware subresources
// and proceeding.
TEST_F(SafeBrowsingBlockingPageTest, PageWithMalwareResourceProceed) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  // Navigate somewhere.
  Navigate(kGoodURL, 1, 0, true);

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(true, kBadURL);

  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Simulate the user clicking "proceed".
  ProceedThroughInterstitial(sb_interstitial);
  EXPECT_EQ(OK, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did proceed, we should be back to showing the page.
  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_EQ(kGoodURL, controller().GetActiveEntry()->GetURL().spec());

  // A report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();
}

// Tests showing a blocking page for a page that contains multiple malware
// subresources and not proceeding.  This just tests that the extra malware
// subresources (which trigger queued interstitial pages) do not break anything.
TEST_F(SafeBrowsingBlockingPageTest,
       PageWithMultipleMalwareResourceDontProceed) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  // Navigate somewhere.
  Navigate(kGoogleURL, 1, 0, true);

  // Navigate somewhere else.
  Navigate(kGoodURL, 2, 0, true);

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(true, kBadURL);

  // More bad resources loading causing more interstitials. The new
  // interstitials should be queued.
  ShowInterstitial(true, kBadURL2);
  ShowInterstitial(true, kBadURL3);

  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Simulate the user clicking "don't proceed".
  DontProceedThroughSubresourceInterstitial(sb_interstitial);
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did not proceed, we should be back to the first page, the 2nd one should
  // have been removed from the navigation controller.
  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_EQ(kGoogleURL, controller().GetActiveEntry()->GetURL().spec());

  // A report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();
}

// Tests showing a blocking page for a page that contains multiple malware
// subresources and proceeding through the first interstitial, but not the next.
TEST_F(SafeBrowsingBlockingPageTest,
       PageWithMultipleMalwareResourceProceedThenDontProceed) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  // Navigate somewhere.
  Navigate(kGoogleURL, 1, 0, true);

  // Navigate somewhere else.
  Navigate(kGoodURL, 2, 0, true);

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(true, kBadURL);

  // More bad resources loading causing more interstitials. The new
  // interstitials should be queued.
  ShowInterstitial(true, kBadURL2);
  ShowInterstitial(true, kBadURL3);

  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Proceed through the 1st interstitial.
  ProceedThroughInterstitial(sb_interstitial);
  EXPECT_EQ(OK, user_response());

  // A report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();

  ResetUserResponse();

  // We should land to a 2nd interstitial (aggregating all the malware resources
  // loaded while the 1st interstitial was showing).
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Don't proceed through the 2nd interstitial.
  DontProceedThroughSubresourceInterstitial(sb_interstitial);
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did not proceed, we should be back to the first page, the 2nd one should
  // have been removed from the navigation controller.
  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_EQ(kGoogleURL, controller().GetActiveEntry()->GetURL().spec());

  // No report should have been sent -- we don't create a report the
  // second time.
  EXPECT_EQ(0u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();
}

// Tests showing a blocking page for a page that contains multiple malware
// subresources and proceeding through the multiple interstitials.
TEST_F(SafeBrowsingBlockingPageTest, PageWithMultipleMalwareResourceProceed) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  // Navigate somewhere else.
  Navigate(kGoodURL, 1, 0, true);

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(true, kBadURL);

  // More bad resources loading causing more interstitials. The new
  // interstitials should be queued.
  ShowInterstitial(true, kBadURL2);
  ShowInterstitial(true, kBadURL3);

  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Proceed through the 1st interstitial.
  ProceedThroughInterstitial(sb_interstitial);
  EXPECT_EQ(OK, user_response());

  // A report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();

  ResetUserResponse();

  // We should land to a 2nd interstitial (aggregating all the malware resources
  // loaded while the 1st interstitial was showing).
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Proceed through the 2nd interstitial.
  ProceedThroughInterstitial(sb_interstitial);
  EXPECT_EQ(OK, user_response());

  // We did proceed, we should be back to the initial page.
  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_EQ(kGoodURL, controller().GetActiveEntry()->GetURL().spec());

  // No report should have been sent -- we don't create a report the
  // second time.
  EXPECT_EQ(0u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();
}

// Tests showing a blocking page then navigating back and forth to make sure the
// controller entries are OK.  http://crbug.com/17627
TEST_F(SafeBrowsingBlockingPageTest, NavigatingBackAndForth) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  // Navigate somewhere.
  Navigate(kGoodURL, 1, 0, true);

  // Now navigate to a bad page triggerring an interstitial.
  controller().LoadURL(GURL(kBadURL), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());
  int pending_id = controller().GetPendingEntry()->GetUniqueID();
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Proceed, then navigate back.
  ProceedThroughInterstitial(sb_interstitial);
  NavigateCrossSite(kBadURL, 2, pending_id, true);  // Commit the navigation.
  GoBack(true);

  // We are back on the good page.
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_FALSE(sb_interstitial);
  ASSERT_EQ(2, controller().GetEntryCount());
  EXPECT_EQ(kGoodURL, controller().GetActiveEntry()->GetURL().spec());

  // Navigate forward to the malware URL.
  web_contents()->GetController().GoForward();
  pending_id = controller().GetPendingEntry()->GetUniqueID();
  ShowInterstitial(false, kBadURL);
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Let's proceed and make sure everything is OK (bug 17627).
  ProceedThroughInterstitial(sb_interstitial);
  // Commit the navigation.
  NavigateCrossSite(kBadURL, 2, pending_id, false);
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_FALSE(sb_interstitial);
  ASSERT_EQ(2, controller().GetEntryCount());
  EXPECT_EQ(kBadURL, controller().GetActiveEntry()->GetURL().spec());

  // Two reports should have been sent.
  EXPECT_EQ(2u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();
}

// Tests that calling "don't proceed" after "proceed" has been called doesn't
// cause problems. http://crbug.com/30079
TEST_F(SafeBrowsingBlockingPageTest, ProceedThenDontProceed) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  // Start a load.
  controller().LoadURL(GURL(kBadURL), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "proceed" then "don't proceed" (before the
  // interstitial is shown).
  sb_interstitial->interstitial_page()->Proceed();
  sb_interstitial->interstitial_page()->DontProceed();
  // Proceed() and DontProceed() post a task to update the
  // SafeBrowsingService::Client.
  base::RunLoop().RunUntilIdle();

  // The interstitial should be gone.
  EXPECT_EQ(OK, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // Only one report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();
}

// Tests showing a blocking page for a malware page with reports disabled.
TEST_F(SafeBrowsingBlockingPageTest, MalwareReportsDisabled) {
  // Disable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, false);

  // Start a load.
  controller().LoadURL(GURL(kBadURL), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
  EXPECT_TRUE(sb_interstitial->CanShowThreatDetailsOption());

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(sb_interstitial);

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did not proceed, the pending entry should be gone.
  EXPECT_FALSE(controller().GetPendingEntry());

  // No report should have been sent.
  EXPECT_EQ(0u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();
}

// Test that toggling the checkbox has the anticipated effects.
TEST_F(SafeBrowsingBlockingPageTest, MalwareReportsToggling) {
  // Disable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, false);

  // Start a load.
  controller().LoadURL(GURL(kBadURL), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
  EXPECT_TRUE(sb_interstitial->CanShowThreatDetailsOption());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled));

  // Simulate the user check the report agreement checkbox.
  sb_interstitial->SetReportingPreference(true);

  EXPECT_TRUE(profile->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled));

  // Simulate the user uncheck the report agreement checkbox.
  sb_interstitial->SetReportingPreference(false);

  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled));
}

// Test that extended reporting option is not shown on blocking an HTTPS main
// page, and no report is sent.
TEST_F(SafeBrowsingBlockingPageTest, ExtendedReportingNotShownOnSecurePage) {
  // Enable malware details.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  // Start a load.
  controller().LoadURL(GURL(kBadHTTPSURL), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadHTTPSURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
  EXPECT_FALSE(sb_interstitial->CanShowThreatDetailsOption());

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(sb_interstitial);

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // No report should have been sent.
  EXPECT_EQ(0u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();
}

// Test that extended reporting option is not shown on blocking an HTTPS
// subresource on an HTTPS page, and no report is sent.
TEST_F(SafeBrowsingBlockingPageTest,
       ExtendedReportingNotShownOnSecurePageWithSecureSubresource) {
  // Enable malware details.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  // Commit a load.
  content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL(kGoodHTTPSURL));

  // Simulate a subresource load causing a safe browsing interstitial to be
  // shown.
  ShowInterstitial(true, kBadHTTPSURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
  EXPECT_FALSE(sb_interstitial->CanShowThreatDetailsOption());

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(sb_interstitial);

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // No report should have been sent.
  EXPECT_EQ(0u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();
}

// Test that extended reporting option is not shown on blocking an HTTP
// subresource on an HTTPS page, and no report is sent.
TEST_F(SafeBrowsingBlockingPageTest,
       ExtendedReportingNotShownOnSecurePageWithInsecureSubresource) {
  // Enable malware details.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  // Commit a load.
  content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL(kGoodHTTPSURL));

  // Simulate a subresource load causing a safe browsing interstitial to be
  // shown.
  ShowInterstitial(true, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
  EXPECT_FALSE(sb_interstitial->CanShowThreatDetailsOption());

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(sb_interstitial);

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // No report should have been sent.
  EXPECT_EQ(0u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();
}

// Test that extended reporting option is shown on blocking an HTTPS
// subresource on an HTTP page.
TEST_F(SafeBrowsingBlockingPageTest,
       ExtendedReportingOnInsecurePageWithSecureSubresource) {
  // Enable malware details.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  // Commit a load.
  content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL(kGoodURL));

  // Simulate a subresource load causing a safe browsing interstitial to be
  // shown.
  ShowInterstitial(true, kBadHTTPSURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
  EXPECT_TRUE(sb_interstitial->CanShowThreatDetailsOption());

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(sb_interstitial);

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // A report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();
}

// Test that extended reporting option is not shown on blocking an HTTPS
// subresource on an HTTPS page while there is a pending load for an HTTP page,
// and no report is sent.
TEST_F(SafeBrowsingBlockingPageTest,
       ExtendedReportingNotShownOnSecurePageWithPendingInsecureLoad) {
  // Enable malware details.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  // Commit a load.
  content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL(kGoodHTTPSURL));

  GURL pending_url("http://slow.example.com");

  // Start a pending load.
  content::WebContentsTester::For(web_contents())->StartNavigation(pending_url);

  // Simulate a subresource load on the committed page causing a safe browsing
  // interstitial to be shown.
  ShowInterstitial(true, kBadHTTPSURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
  // Threat details option should not be shown. (The blocking page is for the
  // committed HTTPS page, not the pending HTTP page.)
  EXPECT_FALSE(sb_interstitial->CanShowThreatDetailsOption());

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(sb_interstitial);

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // No report should have been sent.
  EXPECT_EQ(0u, ui_manager_->GetDetails()->size());
  ui_manager_->GetDetails()->clear();
}

// TODO(mattm): Add test for extended reporting not shown or sent in incognito
// window.

}  // namespace safe_browsing
