// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/malware_details.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
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
static const char* kBadURL = "http://www.badguys.com/";
static const char* kBadURL2 = "http://www.badguys2.com/";
static const char* kBadURL3 = "http://www.badguys3.com/";

namespace {

// A SafeBrowingBlockingPage class that does not create windows.
class TestSafeBrowsingBlockingPage : public SafeBrowsingBlockingPage {
 public:
  TestSafeBrowsingBlockingPage(SafeBrowsingUIManager* manager,
                                 WebContents* web_contents,
                                 const UnsafeResourceList& unsafe_resources)
      : SafeBrowsingBlockingPage(manager, web_contents, unsafe_resources) {
    // Don't delay details at all for the unittest.
    malware_details_proceed_delay_ms_ = 0;
    DontCreateViewForTesting();
  }
};

class TestSafeBrowsingUIManager: public SafeBrowsingUIManager {
 public:
  explicit TestSafeBrowsingUIManager(SafeBrowsingService* service)
      : SafeBrowsingUIManager(service) {
  }

  virtual void SendSerializedMalwareDetails(
      const std::string& serialized) OVERRIDE {
    details_.push_back(serialized);
  }

  std::list<std::string>* GetDetails() {
    return &details_;
  }

 private:
  virtual ~TestSafeBrowsingUIManager() {}

  std::list<std::string> details_;
};

class TestSafeBrowsingBlockingPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  TestSafeBrowsingBlockingPageFactory() { }
  virtual ~TestSafeBrowsingBlockingPageFactory() { }

  virtual SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      SafeBrowsingUIManager* manager,
      WebContents* web_contents,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      OVERRIDE {
    return new TestSafeBrowsingBlockingPage(manager, web_contents,
                                              unsafe_resources);
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

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    SafeBrowsingBlockingPage::RegisterFactory(&factory_);
    ResetUserResponse();
  }

  virtual void TearDown() OVERRIDE {
    // Release the UI manager before the BrowserThreads are destroyed.
    ui_manager_ = NULL;
    SafeBrowsingBlockingPage::RegisterFactory(NULL);
    // Clean up singleton reference (crbug.com/110594).
    MalwareDetails::RegisterFactory(NULL);
    ChromeRenderViewHostTestHarness::TearDown();
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
        GURL(entry->GetURL()),
        content::PAGE_TRANSITION_TYPED);
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
    sb_interstitial->interstitial_page_->Proceed();
    // Proceed() posts a task to update the SafeBrowsingService::Client.
    base::RunLoop().RunUntilIdle();
  }

  static void DontProceedThroughInterstitial(
      SafeBrowsingBlockingPage* sb_interstitial) {
    sb_interstitial->interstitial_page_->DontProceed();
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
    resource->url = url;
    resource->is_subresource = is_subresource;
    resource->threat_type = SB_THREAT_TYPE_URL_MALWARE;
    resource->render_process_host_id =
        web_contents()->GetRenderProcessHost()->GetID();
    resource->render_view_id =
        web_contents()->GetRenderViewHost()->GetRoutingID();
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
                       content::PAGE_TRANSITION_TYPED, std::string());


  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial =
      GetSafeBrowsingBlockingPage();
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
                       content::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial =
      GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Simulate the user clicking "proceed".
  ProceedThroughInterstitial(sb_interstitial);

  // The interstitial is shown until the navigation commits.
  ASSERT_TRUE(InterstitialPage::GetInterstitialPage(web_contents()));
  // Commit the navigation.
  Navigate(kBadURL, 1);
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
  Navigate(kGoogleURL, 1);

  // Navigate somewhere else.
  Navigate(kGoodURL, 2);

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(true, kBadURL);

  SafeBrowsingBlockingPage* sb_interstitial =
      GetSafeBrowsingBlockingPage();
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
  Navigate(kGoodURL, 1);

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(true, kBadURL);

  SafeBrowsingBlockingPage* sb_interstitial =
      GetSafeBrowsingBlockingPage();
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
  Navigate(kGoogleURL, 1);

  // Navigate somewhere else.
  Navigate(kGoodURL, 2);

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(true, kBadURL);

  // More bad resources loading causing more interstitials. The new
  // interstitials should be queued.
  ShowInterstitial(true, kBadURL2);
  ShowInterstitial(true, kBadURL3);

  SafeBrowsingBlockingPage* sb_interstitial =
      GetSafeBrowsingBlockingPage();
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
  Navigate(kGoogleURL, 1);

  // Navigate somewhere else.
  Navigate(kGoodURL, 2);

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(true, kBadURL);

  // More bad resources loading causing more interstitials. The new
  // interstitials should be queued.
  ShowInterstitial(true, kBadURL2);
  ShowInterstitial(true, kBadURL3);

  SafeBrowsingBlockingPage* sb_interstitial =
      GetSafeBrowsingBlockingPage();
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
TEST_F(SafeBrowsingBlockingPageTest,
    PageWithMultipleMalwareResourceProceed) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  // Navigate somewhere else.
  Navigate(kGoodURL, 1);

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(true, kBadURL);

  // More bad resources loading causing more interstitials. The new
  // interstitials should be queued.
  ShowInterstitial(true, kBadURL2);
  ShowInterstitial(true, kBadURL3);

  SafeBrowsingBlockingPage* sb_interstitial =
      GetSafeBrowsingBlockingPage();
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
  Navigate(kGoodURL, 1);

  // Now navigate to a bad page triggerring an interstitial.
  controller().LoadURL(GURL(kBadURL), content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial =
      GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Proceed, then navigate back.
  ProceedThroughInterstitial(sb_interstitial);
  Navigate(kBadURL, 2);  // Commit the navigation.
  GoBack(true);

  // We are back on the good page.
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_FALSE(sb_interstitial);
  ASSERT_EQ(2, controller().GetEntryCount());
  EXPECT_EQ(kGoodURL, controller().GetActiveEntry()->GetURL().spec());

  // Navigate forward to the malware URL.
  web_contents()->GetController().GoForward();
  ShowInterstitial(false, kBadURL);
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Let's proceed and make sure everything is OK (bug 17627).
  ProceedThroughInterstitial(sb_interstitial);
  Navigate(kBadURL, 2);  // Commit the navigation.
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
                       content::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial =
      GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "proceed" then "don't proceed" (before the
  // interstitial is shown).
  sb_interstitial->interstitial_page_->Proceed();
  sb_interstitial->interstitial_page_->DontProceed();
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
                       content::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial =
      GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

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
                       content::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial =
      GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

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

// Test that the transition from old to new preference works.
TEST_F(SafeBrowsingBlockingPageTest, MalwareReportsTransitionEnabled) {
  // The old pref is enabled.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(prefs::kSafeBrowsingReportingEnabled, true);

  // Start a load.
  controller().LoadURL(GURL(kBadURL), content::Referrer(),
                             content::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial =
      GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  base::RunLoop().RunUntilIdle();

  // At this point, nothing should have changed yet.
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled));

  ProceedThroughInterstitial(sb_interstitial);

  // Since the user has proceeded without changing the checkbox, the new pref
  // has been updated.
  EXPECT_TRUE(profile->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled));
}

// Test that the transition from old to new preference still respects the
// user's checkbox preferences.
TEST_F(SafeBrowsingBlockingPageTest, MalwareReportsTransitionDisabled) {
  // The old pref is enabled.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(prefs::kSafeBrowsingReportingEnabled, true);

  // Start a load.
  controller().LoadURL(GURL(kBadURL), content::Referrer(),
                             content::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial =
      GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  base::RunLoop().RunUntilIdle();

  // At this point, nothing should have changed yet.
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled));

  // Simulate the user uncheck the report agreement checkbox.
  sb_interstitial->SetReportingPreference(false);

  ProceedThroughInterstitial(sb_interstitial);

  // The new pref is turned off.
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled));
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingReportingEnabled));
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingDownloadFeedbackEnabled));
}
