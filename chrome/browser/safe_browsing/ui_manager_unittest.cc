// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;

static const char* kGoodURL = "https://www.good.com";
static const char* kBadURL = "https://www.malware.com";
static const char* kBadURLWithPath = "https://www.malware.com/index.html";

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
    ui_manager_->AddToWhitelist(resource);
  }

  SafeBrowsingUIManager::UnsafeResource MakeUnsafeResource(const char* url) {
    SafeBrowsingUIManager::UnsafeResource resource;
    resource.url = GURL(url);
    resource.render_process_host_id =
        web_contents()->GetRenderProcessHost()->GetID();
    resource.render_view_id =
        web_contents()->GetRenderViewHost()->GetRoutingID();
    resource.threat_type = SB_THREAT_TYPE_URL_MALWARE;
    return resource;
  }

  SafeBrowsingUIManager::UnsafeResource MakeUnsafeResourceAndNavigate(
      const char* url) {
    SafeBrowsingUIManager::UnsafeResource resource = MakeUnsafeResource(url);

    // The WC doesn't have a URL without a navigation. Normally the
    // interstitial would provide this instead of a fully committed navigation.
    EXPECT_FALSE(IsWhitelisted(resource));
    NavigateAndCommit(GURL(url));
    return resource;
  }

  void SimulateBlockingPageDone(
      const std::vector<SafeBrowsingUIManager::UnsafeResource>& resources,
      bool proceed) {
    ui_manager_->OnBlockingPageDone(resources, proceed);
  }

 private:
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
};

TEST_F(SafeBrowsingUIManagerTest, Whitelist) {
  SafeBrowsingUIManager::UnsafeResource resource =
      MakeUnsafeResourceAndNavigate(kBadURL);
  AddToWhitelist(resource);
  EXPECT_TRUE(IsWhitelisted(resource));
}

TEST_F(SafeBrowsingUIManagerTest, WhitelistIgnoresSitesNotAdded) {
  SafeBrowsingUIManager::UnsafeResource resource =
      MakeUnsafeResourceAndNavigate(kGoodURL);
  EXPECT_FALSE(IsWhitelisted(resource));
}

TEST_F(SafeBrowsingUIManagerTest, WhitelistIgnoresPath) {
  SafeBrowsingUIManager::UnsafeResource resource =
      MakeUnsafeResourceAndNavigate(kBadURL);
  AddToWhitelist(resource);
  EXPECT_TRUE(IsWhitelisted(resource));

  SafeBrowsingUIManager::UnsafeResource resource_path =
      MakeUnsafeResource(kBadURLWithPath);
  EXPECT_TRUE(IsWhitelisted(resource_path));
}

TEST_F(SafeBrowsingUIManagerTest, WhitelistIgnoresThreatType) {
  SafeBrowsingUIManager::UnsafeResource resource =
      MakeUnsafeResourceAndNavigate(kBadURL);
  AddToWhitelist(resource);
  EXPECT_TRUE(IsWhitelisted(resource));

  SafeBrowsingUIManager::UnsafeResource resource_phishing =
      MakeUnsafeResource(kBadURL);
  resource_phishing.threat_type = SB_THREAT_TYPE_URL_PHISHING;
  EXPECT_TRUE(IsWhitelisted(resource_phishing));
}

TEST_F(SafeBrowsingUIManagerTest, UICallbackProceed) {
  SafeBrowsingUIManager::UnsafeResource resource =
      MakeUnsafeResourceAndNavigate(kBadURL);
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::Bind(&SafeBrowsingCallbackWaiter::OnBlockingPageDone,
                 base::Unretained(&waiter));
  resource.callback_thread =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
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
      MakeUnsafeResourceAndNavigate(kBadURL);
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::Bind(&SafeBrowsingCallbackWaiter::OnBlockingPageDone,
                 base::Unretained(&waiter));
  resource.callback_thread =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
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
      MakeUnsafeResourceAndNavigate(kBadURL);
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::Bind(&SafeBrowsingCallbackWaiter::OnBlockingPageDoneOnIO,
                 base::Unretained(&waiter));
  resource.callback_thread =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
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
      MakeUnsafeResourceAndNavigate(kBadURL);
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::Bind(&SafeBrowsingCallbackWaiter::OnBlockingPageDoneOnIO,
                 base::Unretained(&waiter));
  resource.callback_thread =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
  std::vector<SafeBrowsingUIManager::UnsafeResource> resources;
  resources.push_back(resource);
  SimulateBlockingPageDone(resources, false);
  EXPECT_FALSE(IsWhitelisted(resource));
  waiter.WaitForCallback();
  EXPECT_TRUE(waiter.callback_called());
  EXPECT_FALSE(waiter.proceed());
}

}  // namespace safe_browsing
