// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/browser_thread.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/malware_details.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/test_tab_contents.h"

static const char* kGoogleURL = "http://www.google.com/";
static const char* kGoodURL = "http://www.goodguys.com/";
static const char* kBadURL = "http://www.badguys.com/";
static const char* kBadURL2 = "http://www.badguys2.com/";
static const char* kBadURL3 = "http://www.badguys3.com/";

// A SafeBrowingBlockingPage class that does not create windows.
class TestSafeBrowsingBlockingPage :  public SafeBrowsingBlockingPage {
 public:
  TestSafeBrowsingBlockingPage(SafeBrowsingService* service,
                               TabContents* tab_contents,
                               const UnsafeResourceList& unsafe_resources)
      : SafeBrowsingBlockingPage(service, tab_contents, unsafe_resources) {
  }

  // Overriden from InterstitialPage.  Don't create a view.
  virtual TabContentsView* CreateTabContentsView() {
    return NULL;
  }
};

class TestSafeBrowsingService: public SafeBrowsingService {
 public:
  virtual ~TestSafeBrowsingService() {}
  virtual void ReportMalwareDetails(scoped_refptr<MalwareDetails> details) {
    details_.push_back(details);
  }

  std::list<scoped_refptr<MalwareDetails> >* GetDetails() {
    return &details_;
  }

  std::list<scoped_refptr<MalwareDetails> > details_;
};

class TestSafeBrowsingBlockingPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  TestSafeBrowsingBlockingPageFactory() { }
  ~TestSafeBrowsingBlockingPageFactory() { }

  virtual SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      SafeBrowsingService* service,
      TabContents* tab_contents,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources) {
    return new TestSafeBrowsingBlockingPage(service, tab_contents,
                                            unsafe_resources);
  }
};

class SafeBrowsingBlockingPageTest : public RenderViewHostTestHarness,
                                     public SafeBrowsingService::Client {
 public:
  // The decision the user made.
  enum UserResponse {
    PENDING,
    OK,
    CANCEL
  };

  SafeBrowsingBlockingPageTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()),
        io_thread_(BrowserThread::IO, MessageLoop::current()) {
    ResetUserResponse();
    service_ = new TestSafeBrowsingService();
  }

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    SafeBrowsingBlockingPage::RegisterFactory(&factory_);
    ResetUserResponse();
  }

  // SafeBrowsingService::Client implementation.
  virtual void OnUrlCheckResult(const GURL& url,
                                SafeBrowsingService::UrlCheckResult result) {
  }
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

  void GoBack() {
    NavigationEntry* entry = contents()->controller().GetEntryAtOffset(-1);
    ASSERT_TRUE(entry);
    contents()->controller().GoBack();
    Navigate(entry->url().spec().c_str(), entry->page_id());
  }

  void ShowInterstitial(ResourceType::Type resource_type,
                        const char* url) {
    SafeBrowsingService::UnsafeResource resource;
    InitResource(&resource, resource_type, GURL(url));
    SafeBrowsingBlockingPage::ShowBlockingPage(service_, resource);
  }

  // Returns the SafeBrowsingBlockingPage currently showing or NULL if none is
  // showing.
  SafeBrowsingBlockingPage* GetSafeBrowsingBlockingPage() {
    InterstitialPage* interstitial =
        InterstitialPage::GetInterstitialPage(contents());
    if (!interstitial)
      return NULL;
    return  static_cast<SafeBrowsingBlockingPage*>(interstitial);
  }

  UserResponse user_response() const { return user_response_; }
  void ResetUserResponse() { user_response_ = PENDING; }

  static void ProceedThroughInterstitial(
      SafeBrowsingBlockingPage* sb_interstitial) {
    sb_interstitial->Proceed();
    // Proceed() posts a task to update the SafeBrowsingService::Client.
    MessageLoop::current()->RunAllPending();
  }

  static void DontProceedThroughInterstitial(
      SafeBrowsingBlockingPage* sb_interstitial) {
    sb_interstitial->DontProceed();
    // DontProceed() posts a task to update the SafeBrowsingService::Client.
    MessageLoop::current()->RunAllPending();
  }

  scoped_refptr<TestSafeBrowsingService> service_;

 private:
  void InitResource(SafeBrowsingService::UnsafeResource* resource,
                    ResourceType::Type resource_type,
                    const GURL& url) {
    resource->client = this;
    resource->url = url;
    resource->resource_type = resource_type;
    resource->threat_type = SafeBrowsingService::URL_MALWARE;
    resource->render_process_host_id = contents_->GetRenderProcessHost()->id();
    resource->render_view_id = contents_->render_view_host()->routing_id();
  }

  UserResponse user_response_;
  TestSafeBrowsingBlockingPageFactory factory_;
  BrowserThread ui_thread_;
  BrowserThread io_thread_;
};

// Tests showing a blocking page for a malware page and not proceeding.
TEST_F(SafeBrowsingBlockingPageTest, MalwarePageDontProceed) {
  // Enable malware details.
  contents()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingReportingEnabled, true);

  // Start a load.
  controller().LoadURL(GURL(kBadURL), GURL(), PageTransition::TYPED);


  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(ResourceType::MAIN_FRAME, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  MessageLoop::current()->RunAllPending();

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(sb_interstitial);

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did not proceed, the pending entry should be gone.
  EXPECT_FALSE(controller().pending_entry());

  // A report should have been sent.
  EXPECT_EQ(1u, service_->GetDetails()->size());
  service_->GetDetails()->clear();
}

// Tests showing a blocking page for a malware page and then proceeding.
TEST_F(SafeBrowsingBlockingPageTest, MalwarePageProceed) {
  // Enable malware reports.
  contents()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingReportingEnabled, true);

  // Start a load.
  controller().LoadURL(GURL(kBadURL), GURL(), PageTransition::TYPED);

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(ResourceType::MAIN_FRAME, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Simulate the user clicking "proceed".
  ProceedThroughInterstitial(sb_interstitial);

  // The interstitial is shown until the navigation commits.
  ASSERT_TRUE(InterstitialPage::GetInterstitialPage(contents()));
  // Commit the navigation.
  Navigate(kBadURL, 1);
  // The interstitial should be gone now.
  ASSERT_FALSE(InterstitialPage::GetInterstitialPage(contents()));

  // A report should have been sent.
  EXPECT_EQ(1u, service_->GetDetails()->size());
  service_->GetDetails()->clear();
}

// Tests showing a blocking page for a page that contains malware subresources
// and not proceeding.
TEST_F(SafeBrowsingBlockingPageTest, PageWithMalwareResourceDontProceed) {
  // Enable malware reports.
  contents()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingReportingEnabled, true);

  // Navigate somewhere.
  Navigate(kGoogleURL, 1);

  // Navigate somewhere else.
  Navigate(kGoodURL, 2);

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(ResourceType::SUB_RESOURCE, kBadURL);

  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(sb_interstitial);
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did not proceed, we should be back to the first page, the 2nd one should
  // have been removed from the navigation controller.
  ASSERT_EQ(1, controller().entry_count());
  EXPECT_EQ(kGoogleURL, controller().GetActiveEntry()->url().spec());

  // A report should have been sent.
  EXPECT_EQ(1u, service_->GetDetails()->size());
  service_->GetDetails()->clear();
}

// Tests showing a blocking page for a page that contains malware subresources
// and proceeding.
TEST_F(SafeBrowsingBlockingPageTest, PageWithMalwareResourceProceed) {
  // Enable malware reports.
  contents()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingReportingEnabled, true);

  // Navigate somewhere.
  Navigate(kGoodURL, 1);

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(ResourceType::SUB_RESOURCE, kBadURL);

  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Simulate the user clicking "proceed".
  ProceedThroughInterstitial(sb_interstitial);
  EXPECT_EQ(OK, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did proceed, we should be back to showing the page.
  ASSERT_EQ(1, controller().entry_count());
  EXPECT_EQ(kGoodURL, controller().GetActiveEntry()->url().spec());

  // A report should have been sent.
  EXPECT_EQ(1u, service_->GetDetails()->size());
  service_->GetDetails()->clear();
}

// Tests showing a blocking page for a page that contains multiple malware
// subresources and not proceeding.  This just tests that the extra malware
// subresources (which trigger queued interstitial pages) do not break anything.
TEST_F(SafeBrowsingBlockingPageTest,
       PageWithMultipleMalwareResourceDontProceed) {
  // Enable malware reports.
  contents()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingReportingEnabled, true);

  // Navigate somewhere.
  Navigate(kGoogleURL, 1);

  // Navigate somewhere else.
  Navigate(kGoodURL, 2);

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(ResourceType::SUB_RESOURCE, kBadURL);

  // More bad resources loading causing more interstitials. The new
  // interstitials should be queued.
  ShowInterstitial(ResourceType::SUB_RESOURCE, kBadURL2);
  ShowInterstitial(ResourceType::SUB_RESOURCE, kBadURL3);

  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(sb_interstitial);
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did not proceed, we should be back to the first page, the 2nd one should
  // have been removed from the navigation controller.
  ASSERT_EQ(1, controller().entry_count());
  EXPECT_EQ(kGoogleURL, controller().GetActiveEntry()->url().spec());

  // A report should have been sent.
  EXPECT_EQ(1u, service_->GetDetails()->size());
  service_->GetDetails()->clear();
}

// Tests showing a blocking page for a page that contains multiple malware
// subresources and proceeding through the first interstitial, but not the next.
TEST_F(SafeBrowsingBlockingPageTest,
       PageWithMultipleMalwareResourceProceedThenDontProceed) {
  // Enable malware reports.
  contents()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingReportingEnabled, true);

  // Navigate somewhere.
  Navigate(kGoogleURL, 1);

  // Navigate somewhere else.
  Navigate(kGoodURL, 2);

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(ResourceType::SUB_RESOURCE, kBadURL);

  // More bad resources loading causing more interstitials. The new
  // interstitials should be queued.
  ShowInterstitial(ResourceType::SUB_RESOURCE, kBadURL2);
  ShowInterstitial(ResourceType::SUB_RESOURCE, kBadURL3);

  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Proceed through the 1st interstitial.
  ProceedThroughInterstitial(sb_interstitial);
  EXPECT_EQ(OK, user_response());

  // A report should have been sent.
  EXPECT_EQ(1u, service_->GetDetails()->size());
  service_->GetDetails()->clear();

  ResetUserResponse();

  // We should land to a 2nd interstitial (aggregating all the malware resources
  // loaded while the 1st interstitial was showing).
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Don't proceed through the 2nd interstitial.
  DontProceedThroughInterstitial(sb_interstitial);
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did not proceed, we should be back to the first page, the 2nd one should
  // have been removed from the navigation controller.
  ASSERT_EQ(1, controller().entry_count());
  EXPECT_EQ(kGoogleURL, controller().GetActiveEntry()->url().spec());

  // No report should have been sent -- we don't create a report the
  // second time.
  EXPECT_EQ(0u, service_->GetDetails()->size());
  service_->GetDetails()->clear();
}

// Tests showing a blocking page for a page that contains multiple malware
// subresources and proceeding through the multiple interstitials.
TEST_F(SafeBrowsingBlockingPageTest, PageWithMultipleMalwareResourceProceed) {
  // Enable malware reports.
  contents()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingReportingEnabled, true);

  // Navigate somewhere else.
  Navigate(kGoodURL, 1);

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(ResourceType::SUB_RESOURCE, kBadURL);

  // More bad resources loading causing more interstitials. The new
  // interstitials should be queued.
  ShowInterstitial(ResourceType::SUB_RESOURCE, kBadURL2);
  ShowInterstitial(ResourceType::SUB_RESOURCE, kBadURL3);

  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Proceed through the 1st interstitial.
  ProceedThroughInterstitial(sb_interstitial);
  EXPECT_EQ(OK, user_response());

  // A report should have been sent.
  EXPECT_EQ(1u, service_->GetDetails()->size());
  service_->GetDetails()->clear();

  ResetUserResponse();

  // We should land to a 2nd interstitial (aggregating all the malware resources
  // loaded while the 1st interstitial was showing).
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Proceed through the 2nd interstitial.
  ProceedThroughInterstitial(sb_interstitial);
  EXPECT_EQ(OK, user_response());

  // We did proceed, we should be back to the initial page.
  ASSERT_EQ(1, controller().entry_count());
  EXPECT_EQ(kGoodURL, controller().GetActiveEntry()->url().spec());

  // No report should have been sent -- we don't create a report the
  // second time.
  EXPECT_EQ(0u, service_->GetDetails()->size());
  service_->GetDetails()->clear();
}

// Tests showing a blocking page then navigating back and forth to make sure the
// controller entries are OK.  http://crbug.com/17627
TEST_F(SafeBrowsingBlockingPageTest, NavigatingBackAndForth) {
  // Enable malware reports.
  contents()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingReportingEnabled, true);

  // Navigate somewhere.
  Navigate(kGoodURL, 1);

  // Now navigate to a bad page triggerring an interstitial.
  controller().LoadURL(GURL(kBadURL), GURL(), PageTransition::TYPED);
  ShowInterstitial(ResourceType::MAIN_FRAME, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Proceed, then navigate back.
  ProceedThroughInterstitial(sb_interstitial);
  Navigate(kBadURL, 2);  // Commit the navigation.
  GoBack();

  // We are back on the good page.
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_FALSE(sb_interstitial);
  ASSERT_EQ(2, controller().entry_count());
  EXPECT_EQ(kGoodURL, controller().GetActiveEntry()->url().spec());

  // Navigate forward to the malware URL.
  contents()->controller().GoForward();
  ShowInterstitial(ResourceType::MAIN_FRAME, kBadURL);
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Let's proceed and make sure everything is OK (bug 17627).
  ProceedThroughInterstitial(sb_interstitial);
  Navigate(kBadURL, 2);  // Commit the navigation.
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_FALSE(sb_interstitial);
  ASSERT_EQ(2, controller().entry_count());
  EXPECT_EQ(kBadURL, controller().GetActiveEntry()->url().spec());

  // Two reports should have been sent.
  EXPECT_EQ(2u, service_->GetDetails()->size());
  service_->GetDetails()->clear();
}

// Tests that calling "don't proceed" after "proceed" has been called doesn't
// cause problems. http://crbug.com/30079
TEST_F(SafeBrowsingBlockingPageTest, ProceedThenDontProceed) {
  // Enable malware reports.
  contents()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingReportingEnabled, true);

  // Start a load.
  controller().LoadURL(GURL(kBadURL), GURL(), PageTransition::TYPED);

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(ResourceType::MAIN_FRAME, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  MessageLoop::current()->RunAllPending();

  // Simulate the user clicking "proceed" then "don't proceed" (before the
  // interstitial is shown).
  sb_interstitial->Proceed();
  sb_interstitial->DontProceed();
  // Proceed() and DontProceed() post a task to update the
  // SafeBrowsingService::Client.
  MessageLoop::current()->RunAllPending();

  // The interstitial should be gone.
  EXPECT_EQ(OK, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // Only one report should have been sent.
  EXPECT_EQ(1u, service_->GetDetails()->size());
  service_->GetDetails()->clear();
}

// Tests showing a blocking page for a malware page with reports disabled.
TEST_F(SafeBrowsingBlockingPageTest, MalwareReportsDisabled) {
  // Disable malware reports.
  contents()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingReportingEnabled, false);

  // Start a load.
  controller().LoadURL(GURL(kBadURL), GURL(), PageTransition::TYPED);

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(ResourceType::MAIN_FRAME, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  MessageLoop::current()->RunAllPending();

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(sb_interstitial);

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did not proceed, the pending entry should be gone.
  EXPECT_FALSE(controller().pending_entry());

  // No report should have been sent.
  EXPECT_EQ(0u, service_->GetDetails()->size());
  service_->GetDetails()->clear();
}

// Test setting the malware report preferance
TEST_F(SafeBrowsingBlockingPageTest, MalwareReports) {
  // Disable malware reports.
  contents()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingReportingEnabled, false);

  // Start a load.
  controller().LoadURL(GURL(kBadURL), GURL(), PageTransition::TYPED);

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(ResourceType::MAIN_FRAME, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(contents()->profile()->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingReportingEnabled));

  // Simulate the user check the report agreement checkbox.
  sb_interstitial->SetReportingPreference(true);

  EXPECT_TRUE(contents()->profile()->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingReportingEnabled));

  // Simulate the user uncheck the report agreement checkbox.
  sb_interstitial->SetReportingPreference(false);

  EXPECT_FALSE(contents()->profile()->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingReportingEnabled));
}
