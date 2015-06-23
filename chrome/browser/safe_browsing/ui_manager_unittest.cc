// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_UI_MANAGER_UNITTEST_CC_
#define CHROME_BROWSER_SAFE_BROWSING_UI_MANAGER_UNITTEST_CC_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class SafeBrowsingUIManagerTest : public testing::Test {
 public:
  SafeBrowsingUIManagerTest() : ui_manager_(new SafeBrowsingUIManager(NULL)) {}
  ~SafeBrowsingUIManagerTest() override{};

  bool IsWhitelisted(SafeBrowsingUIManager::UnsafeResource resource) {
    return ui_manager_->IsWhitelisted(resource);
  }

  void AddToWhitelist(SafeBrowsingUIManager::UnsafeResource resource) {
    ui_manager_->UpdateWhitelist(resource);
  }

 private:
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(SafeBrowsingUIManagerTest, Whitelist) {
  SafeBrowsingUIManager::UnsafeResource resource;
  resource.url = GURL("https://www.google.com");
  resource.render_process_host_id = 14;
  resource.render_view_id = 10;
  resource.threat_type = SB_THREAT_TYPE_URL_MALWARE;

  EXPECT_FALSE(IsWhitelisted(resource));
  AddToWhitelist(resource);
  EXPECT_TRUE(IsWhitelisted(resource));
}

TEST_F(SafeBrowsingUIManagerTest, WhitelistId) {
  SafeBrowsingUIManager::UnsafeResource resource;
  resource.url = GURL("https://www.google.com");
  resource.render_process_host_id = 14;
  resource.render_view_id = 10;
  resource.threat_type = SB_THREAT_TYPE_URL_MALWARE;

  // Different render view ID.
  SafeBrowsingUIManager::UnsafeResource resource_view_id;
  resource_view_id.url = GURL("https://www.google.com");
  resource_view_id.render_process_host_id = 14;
  resource_view_id.render_view_id = 3;
  resource_view_id.threat_type = SB_THREAT_TYPE_URL_MALWARE;

  // Different render process host ID.
  SafeBrowsingUIManager::UnsafeResource resource_process_id;
  resource_process_id.url = GURL("https://www.google.com");
  resource_process_id.render_process_host_id = 6;
  resource_process_id.render_view_id = 10;
  resource_process_id.threat_type = SB_THREAT_TYPE_URL_MALWARE;

  EXPECT_FALSE(IsWhitelisted(resource));
  EXPECT_FALSE(IsWhitelisted(resource_view_id));
  EXPECT_FALSE(IsWhitelisted(resource_process_id));
  AddToWhitelist(resource);
  EXPECT_TRUE(IsWhitelisted(resource));
  EXPECT_FALSE(IsWhitelisted(resource_view_id));
  EXPECT_FALSE(IsWhitelisted(resource_process_id));
}

TEST_F(SafeBrowsingUIManagerTest, WhitelistUrl) {
  SafeBrowsingUIManager::UnsafeResource google;
  google.url = GURL("https://www.google.com");
  google.render_process_host_id = 14;
  google.render_view_id = 10;
  google.threat_type = SB_THREAT_TYPE_URL_MALWARE;

  SafeBrowsingUIManager::UnsafeResource gmail;
  gmail.url = GURL("https://www.gmail.com");
  gmail.render_process_host_id = 14;
  gmail.render_view_id = 10;
  gmail.threat_type = SB_THREAT_TYPE_URL_MALWARE;

  EXPECT_FALSE(IsWhitelisted(google));
  EXPECT_FALSE(IsWhitelisted(gmail));
  AddToWhitelist(google);
  EXPECT_TRUE(IsWhitelisted(google));
  EXPECT_FALSE(IsWhitelisted(gmail));
}

TEST_F(SafeBrowsingUIManagerTest, WhitelistThreat) {
  SafeBrowsingUIManager::UnsafeResource list_malware;
  list_malware.url = GURL("https://www.google.com");
  list_malware.render_process_host_id = 14;
  list_malware.render_view_id = 10;
  list_malware.threat_type = SB_THREAT_TYPE_URL_MALWARE;

  SafeBrowsingUIManager::UnsafeResource client_malware;
  client_malware.url = GURL("https://www.google.com");
  client_malware.render_process_host_id = 14;
  client_malware.render_view_id = 10;
  client_malware.threat_type = SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL;

  SafeBrowsingUIManager::UnsafeResource unwanted;
  unwanted.url = GURL("https://www.google.com");
  unwanted.render_process_host_id = 14;
  unwanted.render_view_id = 10;
  unwanted.threat_type = SB_THREAT_TYPE_URL_UNWANTED;

  SafeBrowsingUIManager::UnsafeResource phishing;
  phishing.url = GURL("https://www.google.com");
  phishing.render_process_host_id = 14;
  phishing.render_view_id = 10;
  phishing.threat_type = SB_THREAT_TYPE_URL_PHISHING;

  SafeBrowsingUIManager::UnsafeResource client_phishing;
  client_phishing.url = GURL("https://www.google.com");
  client_phishing.render_process_host_id = 14;
  client_phishing.render_view_id = 10;
  client_phishing.threat_type = SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL;

  EXPECT_FALSE(IsWhitelisted(list_malware));
  EXPECT_FALSE(IsWhitelisted(client_malware));
  EXPECT_FALSE(IsWhitelisted(unwanted));
  EXPECT_FALSE(IsWhitelisted(phishing));
  EXPECT_FALSE(IsWhitelisted(client_phishing));
  AddToWhitelist(list_malware);
  EXPECT_TRUE(IsWhitelisted(list_malware));
  EXPECT_TRUE(IsWhitelisted(client_malware));
  EXPECT_TRUE(IsWhitelisted(unwanted));
  EXPECT_TRUE(IsWhitelisted(phishing));
  EXPECT_TRUE(IsWhitelisted(client_phishing));
}

#endif  // CHROME_BROWSER_SAFE_BROWSING_UI_MANAGER_UNITTEST_CC_
