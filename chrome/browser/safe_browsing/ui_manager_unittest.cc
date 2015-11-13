// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
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

static const char* kGoodURL = "https://www.good.com";
static const char* kBadURL = "https://www.malware.com";
static const char* kBadURLWithPath = "https://www.malware.com/index.html";

namespace safe_browsing {

class SafeBrowsingUIManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  SafeBrowsingUIManagerTest() : ui_manager_(new SafeBrowsingUIManager(NULL)) {}

  ~SafeBrowsingUIManagerTest() override{};

  void SetUp() override { ChromeRenderViewHostTestHarness::SetUp(); }

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

}  // namespace safe_browsing
