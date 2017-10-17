// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/db/v4_get_hash_interceptor.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/db/safebrowsing.pb.h"
#include "components/safe_browsing/db/util.h"
#include "components/safe_browsing/db/v4_test_util.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace safe_browsing {

// This harness tests test-only code for correctness. This ensures that other
// test classes which want to use the V4 interceptor are testing the right
// thing.
class V4GetHashInterceptorBrowserTest : public InProcessBrowserTest {
 public:
  V4GetHashInterceptorBrowserTest() {}
  ~V4GetHashInterceptorBrowserTest() override {}

  void SetUp() override {
    // We only need to mock a local database. The tests will use a true real V4
    // protocol manager.
    V4Database::RegisterStoreFactoryForTest(
        base::MakeUnique<TestV4StoreFactory>());

    auto v4_db_factory = base::MakeUnique<TestV4DatabaseFactory>();
    v4_db_factory_ = v4_db_factory.get();
    V4Database::RegisterDatabaseFactoryForTest(std::move(v4_db_factory));
    InProcessBrowserTest::SetUp();
  }
  void TearDown() override {
    InProcessBrowserTest::TearDown();
    V4Database::RegisterStoreFactoryForTest(nullptr);
    V4Database::RegisterDatabaseFactoryForTest(nullptr);
  }

  // Only marks the prefix as bad in the local database. The server will respond
  // with the source of truth.
  void LocallyMarkPrefixAsBad(const GURL& url, const ListIdentifier& list_id) {
    FullHash full_hash = GetFullHash(url);
    v4_db_factory_->MarkPrefixAsBad(list_id, full_hash);
  }

 private:
  // Owned by the V4Database.
  TestV4DatabaseFactory* v4_db_factory_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(V4GetHashInterceptorBrowserTest);
};

IN_PROC_BROWSER_TEST_F(V4GetHashInterceptorBrowserTest, SimpleTest) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const char kMalwarePage[] = "/safe_browsing/malware.html";
  const GURL bad_url = embedded_test_server()->GetURL(kMalwarePage);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ui_test_utils::NavigateToURL(browser(), bad_url);
  EXPECT_FALSE(contents->GetInterstitialPage());

  ThreatMatch match;
  FullHash full_hash = GetFullHash(bad_url);
  LocallyMarkPrefixAsBad(bad_url, GetUrlMalwareId());
  match.set_platform_type(GetUrlMalwareId().platform_type());
  match.set_threat_entry_type(ThreatEntryType::URL);
  match.set_threat_type(ThreatType::MALWARE_THREAT);
  match.mutable_threat()->set_hash(full_hash);
  match.mutable_cache_duration()->set_seconds(300);
  V4GetHashInterceptor::Register(
      base::MakeUnique<V4GetHashInterceptor>(bad_url, match),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));

  ui_test_utils::NavigateToURL(browser(), bad_url);
  EXPECT_TRUE(contents->GetInterstitialPage());
}

IN_PROC_BROWSER_TEST_F(V4GetHashInterceptorBrowserTest,
                       WrongFullHash_NoInterstitial) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const char kMalwarePage[] = "/safe_browsing/malware.html";
  const GURL bad_url = embedded_test_server()->GetURL(kMalwarePage);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ui_test_utils::NavigateToURL(browser(), bad_url);
  EXPECT_FALSE(contents->GetInterstitialPage());

  // Return a different full hash, so there will be no match and no
  // interstitial.
  ThreatMatch match;
  FullHash full_hash = GetFullHash(GURL("https://example.test/"));
  LocallyMarkPrefixAsBad(bad_url, GetUrlMalwareId());
  match.set_platform_type(GetUrlMalwareId().platform_type());
  match.set_threat_entry_type(ThreatEntryType::URL);
  match.set_threat_type(ThreatType::MALWARE_THREAT);
  match.mutable_threat()->set_hash(full_hash);
  match.mutable_cache_duration()->set_seconds(300);
  V4GetHashInterceptor::Register(
      base::MakeUnique<V4GetHashInterceptor>(bad_url, match),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));

  ui_test_utils::NavigateToURL(browser(), bad_url);
  EXPECT_FALSE(contents->GetInterstitialPage());
}

}  // namespace safe_browsing
