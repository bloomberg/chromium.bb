// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/appcache/appcache_database.h"
#include "webkit/browser/appcache/appcache_storage_impl.h"
#include "webkit/browser/appcache/appcache_test_helper.h"
#include "webkit/browser/quota/mock_special_storage_policy.h"

#include <set>

using appcache::AppCacheTestHelper;

namespace content {
namespace {
const base::FilePath::CharType kTestingAppCacheDirname[] =
    FILE_PATH_LITERAL("Application Cache");

// Examples of a protected and an unprotected origin, to be used througout the
// test.
const char kProtectedManifest[] = "http://www.protected.com/cache.manifest";
const char kNormalManifest[] = "http://www.normal.com/cache.manifest";
const char kSessionOnlyManifest[] = "http://www.sessiononly.com/cache.manifest";

}  // namespace

class ChromeAppCacheServiceTest : public testing::Test {
 protected:
  ChromeAppCacheServiceTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        kProtectedManifestURL(kProtectedManifest),
        kNormalManifestURL(kNormalManifest),
        kSessionOnlyManifestURL(kSessionOnlyManifest) {
  }

  scoped_refptr<ChromeAppCacheService> CreateAppCacheService(
      const base::FilePath& appcache_path,
      bool init_storage);
  void InsertDataIntoAppCache(ChromeAppCacheService* appcache_service);

  TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  const GURL kProtectedManifestURL;
  const GURL kNormalManifestURL;
  const GURL kSessionOnlyManifestURL;

 private:
  TestBrowserContext browser_context_;
};

scoped_refptr<ChromeAppCacheService>
ChromeAppCacheServiceTest::CreateAppCacheService(
    const base::FilePath& appcache_path,
    bool init_storage) {
  scoped_refptr<ChromeAppCacheService> appcache_service =
      new ChromeAppCacheService(NULL);
  scoped_refptr<quota::MockSpecialStoragePolicy> mock_policy =
      new quota::MockSpecialStoragePolicy;
  mock_policy->AddProtected(kProtectedManifestURL.GetOrigin());
  mock_policy->AddSessionOnly(kSessionOnlyManifestURL.GetOrigin());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ChromeAppCacheService::InitializeOnIOThread,
                 appcache_service.get(),
                 appcache_path,
                 browser_context_.GetResourceContext(),
                 make_scoped_refptr(browser_context_.GetRequestContext()),
                 mock_policy));
  // Steps needed to initialize the storage of AppCache data.
  base::RunLoop().RunUntilIdle();
  if (init_storage) {
    appcache::AppCacheStorageImpl* storage =
        static_cast<appcache::AppCacheStorageImpl*>(
            appcache_service->storage());
    storage->database_->db_connection();
    storage->disk_cache();
    base::RunLoop().RunUntilIdle();
  }
  return appcache_service;
}

void ChromeAppCacheServiceTest::InsertDataIntoAppCache(
    ChromeAppCacheService* appcache_service) {
  AppCacheTestHelper appcache_helper;
  appcache_helper.AddGroupAndCache(appcache_service, kNormalManifestURL);
  appcache_helper.AddGroupAndCache(appcache_service, kProtectedManifestURL);
  appcache_helper.AddGroupAndCache(appcache_service, kSessionOnlyManifestURL);

  // Verify that adding the data succeeded
  std::set<GURL> origins;
  appcache_helper.GetOriginsWithCaches(appcache_service, &origins);
  ASSERT_EQ(3UL, origins.size());
  ASSERT_TRUE(origins.find(kProtectedManifestURL.GetOrigin()) != origins.end());
  ASSERT_TRUE(origins.find(kNormalManifestURL.GetOrigin()) != origins.end());
  ASSERT_TRUE(origins.find(kSessionOnlyManifestURL.GetOrigin()) !=
              origins.end());
}

TEST_F(ChromeAppCacheServiceTest, KeepOnDestruction) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath appcache_path =
      temp_dir_.path().Append(kTestingAppCacheDirname);

  // Create a ChromeAppCacheService and insert data into it
  scoped_refptr<ChromeAppCacheService> appcache_service =
      CreateAppCacheService(appcache_path, true);
  ASSERT_TRUE(base::PathExists(appcache_path));
  ASSERT_TRUE(base::PathExists(appcache_path.AppendASCII("Index")));
  InsertDataIntoAppCache(appcache_service.get());

  // Test: delete the ChromeAppCacheService
  appcache_service = NULL;
  base::RunLoop().RunUntilIdle();

  // Recreate the appcache (for reading the data back)
  appcache_service = CreateAppCacheService(appcache_path, false);

  // The directory is still there
  ASSERT_TRUE(base::PathExists(appcache_path));

  // The appcache data is also there, except the session-only origin.
  AppCacheTestHelper appcache_helper;
  std::set<GURL> origins;
  appcache_helper.GetOriginsWithCaches(appcache_service.get(), &origins);
  EXPECT_EQ(2UL, origins.size());
  EXPECT_TRUE(origins.find(kProtectedManifestURL.GetOrigin()) != origins.end());
  EXPECT_TRUE(origins.find(kNormalManifestURL.GetOrigin()) != origins.end());
  EXPECT_TRUE(origins.find(kSessionOnlyManifestURL.GetOrigin()) ==
              origins.end());

  // Delete and let cleanup tasks run prior to returning.
  appcache_service = NULL;
  base::RunLoop().RunUntilIdle();
}

TEST_F(ChromeAppCacheServiceTest, SaveSessionState) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath appcache_path =
      temp_dir_.path().Append(kTestingAppCacheDirname);

  // Create a ChromeAppCacheService and insert data into it
  scoped_refptr<ChromeAppCacheService> appcache_service =
      CreateAppCacheService(appcache_path, true);
  ASSERT_TRUE(base::PathExists(appcache_path));
  ASSERT_TRUE(base::PathExists(appcache_path.AppendASCII("Index")));
  InsertDataIntoAppCache(appcache_service.get());

  // Save session state. This should bypass the destruction-time deletion.
  appcache_service->set_force_keep_session_state();

  // Test: delete the ChromeAppCacheService
  appcache_service = NULL;
  base::RunLoop().RunUntilIdle();

  // Recreate the appcache (for reading the data back)
  appcache_service = CreateAppCacheService(appcache_path, false);

  // The directory is still there
  ASSERT_TRUE(base::PathExists(appcache_path));

  // No appcache data was deleted.
  AppCacheTestHelper appcache_helper;
  std::set<GURL> origins;
  appcache_helper.GetOriginsWithCaches(appcache_service.get(), &origins);
  EXPECT_EQ(3UL, origins.size());
  EXPECT_TRUE(origins.find(kProtectedManifestURL.GetOrigin()) != origins.end());
  EXPECT_TRUE(origins.find(kNormalManifestURL.GetOrigin()) != origins.end());
  EXPECT_TRUE(origins.find(kSessionOnlyManifestURL.GetOrigin()) !=
              origins.end());

  // Delete and let cleanup tasks run prior to returning.
  appcache_service = NULL;
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
