// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/ui_manager.h"

#include "base/run_loop.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"
#include "components/safe_browsing_db/util.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;

static const char* kGoodURL = "https://www.good.com";
static const char* kBadURL = "https://www.malware.com";
static const char* kBadURLWithPath = "https://www.malware.com/index.html";
static const char* kAnotherBadURL = "https://www.badware.com";
static const char* kLandingURL = "https://www.landing.com";

namespace safe_browsing {

class SafeBrowsingCallbackWaiter {
  public:
   SafeBrowsingCallbackWaiter() {}

   bool callback_called() const { return callback_called_; }
   bool proceed() const { return proceed_; }

   void OnBlockingPageDone(bool proceed) {
     DCHECK_CURRENTLY_ON(BrowserThread::UI);
     callback_called_ = true;
     proceed_ = proceed;
     loop_.Quit();
   }

   void OnBlockingPageDoneOnIO(bool proceed) {
     DCHECK_CURRENTLY_ON(BrowserThread::IO);
     BrowserThread::PostTask(
         BrowserThread::UI, FROM_HERE,
         base::Bind(&SafeBrowsingCallbackWaiter::OnBlockingPageDone,
                    base::Unretained(this), proceed));
   }

   void WaitForCallback() {
     DCHECK_CURRENTLY_ON(BrowserThread::UI);
     loop_.Run();
   }

  private:
   bool callback_called_ = false;
   bool proceed_ = false;
   base::RunLoop loop_;
};

class SafeBrowsingUIManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  SafeBrowsingUIManagerTest() : ui_manager_(new SafeBrowsingUIManager(NULL)) {}

  ~SafeBrowsingUIManagerTest() override{};

  void SetUp() override {
    SetThreadBundleOptions(content::TestBrowserThreadBundle::REAL_IO_THREAD);
    ChromeRenderViewHostTestHarness::SetUp();
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  bool IsWhitelisted(SafeBrowsingUIManager::UnsafeResource resource) {
    return ui_manager_->IsWhitelisted(resource);
  }

  void AddToWhitelist(SafeBrowsingUIManager::UnsafeResource resource) {
    ui_manager_->AddToWhitelistUrlSet(resource, false);
  }

  SafeBrowsingUIManager::UnsafeResource MakeUnsafeResource(
      const char* url,
      bool is_subresource) {
    SafeBrowsingUIManager::UnsafeResource resource;
    resource.url = GURL(url);
    resource.is_subresource = is_subresource;
    resource.web_contents_getter =
        SafeBrowsingUIManager::UnsafeResource::GetWebContentsGetter(
            web_contents()->GetRenderProcessHost()->GetID(),
            web_contents()->GetMainFrame()->GetRoutingID());
    resource.threat_type = SB_THREAT_TYPE_URL_MALWARE;
    return resource;
  }

  SafeBrowsingUIManager::UnsafeResource MakeUnsafeResourceAndStartNavigation(
      const char* url) {
    SafeBrowsingUIManager::UnsafeResource resource =
        MakeUnsafeResource(url, false /* is_subresource */);

    // The WC doesn't have a URL without a navigation. A main-frame malware
    // unsafe resource must be a pending navigation.
    content::WebContentsTester::For(web_contents())->StartNavigation(GURL(url));
    return resource;
  }

  void SimulateBlockingPageDone(
      const std::vector<SafeBrowsingUIManager::UnsafeResource>& resources,
      bool proceed) {
    ui_manager_->OnBlockingPageDone(resources, proceed);
  }

 protected:
  SafeBrowsingUIManager* ui_manager() { return ui_manager_.get(); }

 private:
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
};

TEST_F(SafeBrowsingUIManagerTest, Whitelist) {
  SafeBrowsingUIManager::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  AddToWhitelist(resource);
  EXPECT_TRUE(IsWhitelisted(resource));
}

TEST_F(SafeBrowsingUIManagerTest, WhitelistIgnoresSitesNotAdded) {
  SafeBrowsingUIManager::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kGoodURL);
  EXPECT_FALSE(IsWhitelisted(resource));
}

TEST_F(SafeBrowsingUIManagerTest, WhitelistIgnoresPath) {
  SafeBrowsingUIManager::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  AddToWhitelist(resource);
  EXPECT_TRUE(IsWhitelisted(resource));

  content::WebContentsTester::For(web_contents())->CommitPendingNavigation();

  SafeBrowsingUIManager::UnsafeResource resource_path =
      MakeUnsafeResourceAndStartNavigation(kBadURLWithPath);
  EXPECT_TRUE(IsWhitelisted(resource_path));
}

TEST_F(SafeBrowsingUIManagerTest, WhitelistIgnoresThreatType) {
  SafeBrowsingUIManager::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  AddToWhitelist(resource);
  EXPECT_TRUE(IsWhitelisted(resource));

  SafeBrowsingUIManager::UnsafeResource resource_phishing =
      MakeUnsafeResource(kBadURL, false /* is_subresource */);
  resource_phishing.threat_type = SB_THREAT_TYPE_URL_PHISHING;
  EXPECT_TRUE(IsWhitelisted(resource_phishing));
}

TEST_F(SafeBrowsingUIManagerTest, WhitelistWithUnrelatedPendingLoad) {
  // Commit load of landing page.
  NavigateAndCommit(GURL(kLandingURL));
  {
    // Simulate subresource malware hit on the landing page.
    SafeBrowsingUIManager::UnsafeResource resource =
        MakeUnsafeResource(kBadURL, true /* is_subresource */);

    // Start pending load to unrelated site.
    content::WebContentsTester::For(web_contents())
        ->StartNavigation(GURL(kGoodURL));

    // Whitelist the resource on the landing page.
    AddToWhitelist(resource);
    EXPECT_TRUE(IsWhitelisted(resource));
  }

  // Commit the pending load of unrelated site.
  content::WebContentsTester::For(web_contents())->CommitPendingNavigation();
  {
    // The unrelated site is not on the whitelist, even if the same subresource
    // was on it.
    SafeBrowsingUIManager::UnsafeResource resource =
        MakeUnsafeResource(kBadURL, true /* is_subresource */);
    EXPECT_FALSE(IsWhitelisted(resource));
  }

  // Navigate back to the original landing url.
  NavigateAndCommit(GURL(kLandingURL));
  {
    SafeBrowsingUIManager::UnsafeResource resource =
        MakeUnsafeResource(kBadURL, true /* is_subresource */);
    // Original resource url is whitelisted.
    EXPECT_TRUE(IsWhitelisted(resource));
  }
  {
    // A different malware subresource on the same page is also whitelisted.
    // (The whitelist is by the page url, not the resource url.)
    SafeBrowsingUIManager::UnsafeResource resource2 =
        MakeUnsafeResource(kAnotherBadURL, true /* is_subresource */);
    EXPECT_TRUE(IsWhitelisted(resource2));
  }
}

TEST_F(SafeBrowsingUIManagerTest, UICallbackProceed) {
  SafeBrowsingUIManager::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::Bind(&SafeBrowsingCallbackWaiter::OnBlockingPageDone,
                 base::Unretained(&waiter));
  resource.callback_thread =
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI);
  std::vector<SafeBrowsingUIManager::UnsafeResource> resources;
  resources.push_back(resource);
  SimulateBlockingPageDone(resources, true);
  EXPECT_TRUE(IsWhitelisted(resource));
  waiter.WaitForCallback();
  EXPECT_TRUE(waiter.callback_called());
  EXPECT_TRUE(waiter.proceed());
}

TEST_F(SafeBrowsingUIManagerTest, UICallbackDontProceed) {
  SafeBrowsingUIManager::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::Bind(&SafeBrowsingCallbackWaiter::OnBlockingPageDone,
                 base::Unretained(&waiter));
  resource.callback_thread =
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI);
  std::vector<SafeBrowsingUIManager::UnsafeResource> resources;
  resources.push_back(resource);
  SimulateBlockingPageDone(resources, false);
  EXPECT_FALSE(IsWhitelisted(resource));
  waiter.WaitForCallback();
  EXPECT_TRUE(waiter.callback_called());
  EXPECT_FALSE(waiter.proceed());
}

TEST_F(SafeBrowsingUIManagerTest, IOCallbackProceed) {
  SafeBrowsingUIManager::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::Bind(&SafeBrowsingCallbackWaiter::OnBlockingPageDoneOnIO,
                 base::Unretained(&waiter));
  resource.callback_thread =
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);
  std::vector<SafeBrowsingUIManager::UnsafeResource> resources;
  resources.push_back(resource);
  SimulateBlockingPageDone(resources, true);
  EXPECT_TRUE(IsWhitelisted(resource));
  waiter.WaitForCallback();
  EXPECT_TRUE(waiter.callback_called());
  EXPECT_TRUE(waiter.proceed());
}

TEST_F(SafeBrowsingUIManagerTest, IOCallbackDontProceed) {
  SafeBrowsingUIManager::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::Bind(&SafeBrowsingCallbackWaiter::OnBlockingPageDoneOnIO,
                 base::Unretained(&waiter));
  resource.callback_thread =
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);
  std::vector<SafeBrowsingUIManager::UnsafeResource> resources;
  resources.push_back(resource);
  SimulateBlockingPageDone(resources, false);
  EXPECT_FALSE(IsWhitelisted(resource));
  waiter.WaitForCallback();
  EXPECT_TRUE(waiter.callback_called());
  EXPECT_FALSE(waiter.proceed());
}

namespace {

// A WebContentsDelegate that records whether
// VisibleSecurityStateChanged() was called.
class SecurityStateWebContentsDelegate : public content::WebContentsDelegate {
 public:
  SecurityStateWebContentsDelegate() {}
  ~SecurityStateWebContentsDelegate() override {}

  bool visible_security_state_changed() const {
    return visible_security_state_changed_;
  }

  void ClearVisibleSecurityStateChanged() {
    visible_security_state_changed_ = false;
  }

  // WebContentsDelegate:
  void VisibleSecurityStateChanged(content::WebContents* source) override {
    visible_security_state_changed_ = true;
  }

 private:
  bool visible_security_state_changed_ = false;
  DISALLOW_COPY_AND_ASSIGN(SecurityStateWebContentsDelegate);
};

// A test blocking page that does not create windows.
class TestSafeBrowsingBlockingPage : public SafeBrowsingBlockingPage {
 public:
  TestSafeBrowsingBlockingPage(SafeBrowsingUIManager* manager,
                               content::WebContents* web_contents,
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

// A factory that creates TestSafeBrowsingBlockingPages.
class TestSafeBrowsingBlockingPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  TestSafeBrowsingBlockingPageFactory() {}
  ~TestSafeBrowsingBlockingPageFactory() override {}

  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      SafeBrowsingUIManager* delegate,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      override {
    return new TestSafeBrowsingBlockingPage(delegate, web_contents,
                                            main_frame_url, unsafe_resources);
  }
};

}  // namespace

// Tests that the WebContentsDelegate is notified of a visible security
// state change when a blocking page is shown for a subresource.
TEST_F(SafeBrowsingUIManagerTest,
       VisibleSecurityStateChangedForUnsafeSubresource) {
  TestSafeBrowsingBlockingPageFactory factory;
  SafeBrowsingBlockingPage::RegisterFactory(&factory);
  SecurityStateWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);

  // Simulate a blocking page showing for an unsafe subresource.
  SafeBrowsingUIManager::UnsafeResource resource =
      MakeUnsafeResource(kBadURL, true /* is_subresource */);
  // Needed for showing the blocking page.
  resource.threat_source = safe_browsing::ThreatSource::REMOTE;
  NavigateAndCommit(GURL("http://example.test"));

  delegate.ClearVisibleSecurityStateChanged();
  EXPECT_FALSE(delegate.visible_security_state_changed());
  ui_manager()->DisplayBlockingPage(resource);
  EXPECT_TRUE(delegate.visible_security_state_changed());

  // Simulate proceeding through the blocking page.
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::Bind(&SafeBrowsingCallbackWaiter::OnBlockingPageDoneOnIO,
                 base::Unretained(&waiter));
  resource.callback_thread =
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);
  std::vector<SafeBrowsingUIManager::UnsafeResource> resources;
  resources.push_back(resource);

  delegate.ClearVisibleSecurityStateChanged();
  EXPECT_FALSE(delegate.visible_security_state_changed());
  SimulateBlockingPageDone(resources, true);
  EXPECT_TRUE(delegate.visible_security_state_changed());

  waiter.WaitForCallback();
  EXPECT_TRUE(waiter.callback_called());
  EXPECT_TRUE(waiter.proceed());
  EXPECT_TRUE(IsWhitelisted(resource));
}

}  // namespace safe_browsing
