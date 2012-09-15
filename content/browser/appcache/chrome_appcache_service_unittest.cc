// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/public/browser/resource_context.h"
#include "content/public/test/test_browser_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache_database.h"
#include "webkit/appcache/appcache_storage_impl.h"
#include "webkit/appcache/appcache_test_helper.h"
#include "webkit/quota/mock_special_storage_policy.h"

#include <set>

using content::BrowserThread;
using content::BrowserThreadImpl;

namespace {
const FilePath::CharType kTestingAppCacheDirname[] =
    FILE_PATH_LITERAL("Application Cache");

// Examples of a protected and an unprotected origin, to be used througout the
// test.
const char kProtectedManifest[] = "http://www.protected.com/cache.manifest";
const char kNormalManifest[] = "http://www.normal.com/cache.manifest";
const char kSessionOnlyManifest[] = "http://www.sessiononly.com/cache.manifest";

class MockURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  MockURLRequestContextGetter(
      net::URLRequestContext* context,
      base::MessageLoopProxy* message_loop_proxy)
      : context_(context), message_loop_proxy_(message_loop_proxy) {
  }

  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE {
    return context_;
  }

  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE {
    return message_loop_proxy_;
  }

 protected:
  virtual ~MockURLRequestContextGetter() {}

 private:
  net::URLRequestContext* context_;
  scoped_refptr<base::SingleThreadTaskRunner> message_loop_proxy_;
};

}  // namespace

namespace appcache {

class ChromeAppCacheServiceTest : public testing::Test {
 public:
  ChromeAppCacheServiceTest()
      : message_loop_(MessageLoop::TYPE_IO),
        kProtectedManifestURL(kProtectedManifest),
        kNormalManifestURL(kNormalManifest),
        kSessionOnlyManifestURL(kSessionOnlyManifest),
        file_thread_(BrowserThread::FILE, &message_loop_),
        file_user_blocking_thread_(
            BrowserThread::FILE_USER_BLOCKING, &message_loop_),
        cache_thread_(BrowserThread::CACHE, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {
  }

 protected:
  scoped_refptr<ChromeAppCacheService> CreateAppCacheService(
      const FilePath& appcache_path,
      bool init_storage);
  void InsertDataIntoAppCache(ChromeAppCacheService* appcache_service);

  MessageLoop message_loop_;
  ScopedTempDir temp_dir_;
  const GURL kProtectedManifestURL;
  const GURL kNormalManifestURL;
  const GURL kSessionOnlyManifestURL;

 private:
  BrowserThreadImpl file_thread_;
  BrowserThreadImpl file_user_blocking_thread_;
  BrowserThreadImpl cache_thread_;
  BrowserThreadImpl io_thread_;
  content::TestBrowserContext browser_context_;
};

scoped_refptr<ChromeAppCacheService>
ChromeAppCacheServiceTest::CreateAppCacheService(
    const FilePath& appcache_path,
    bool init_storage) {
  scoped_refptr<ChromeAppCacheService> appcache_service =
      new ChromeAppCacheService(NULL);
  scoped_refptr<quota::MockSpecialStoragePolicy> mock_policy =
      new quota::MockSpecialStoragePolicy;
  mock_policy->AddProtected(kProtectedManifestURL.GetOrigin());
  mock_policy->AddSessionOnly(kSessionOnlyManifestURL.GetOrigin());
  scoped_refptr<MockURLRequestContextGetter> mock_request_context_getter =
      new MockURLRequestContextGetter(
          browser_context_.GetResourceContext()->GetRequestContext(),
          message_loop_.message_loop_proxy());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ChromeAppCacheService::InitializeOnIOThread,
                 appcache_service.get(), appcache_path,
                 browser_context_.GetResourceContext(),
                 mock_request_context_getter,
                 mock_policy));
  // Steps needed to initialize the storage of AppCache data.
  message_loop_.RunAllPending();
  if (init_storage) {
    appcache::AppCacheStorageImpl* storage =
        static_cast<appcache::AppCacheStorageImpl*>(
            appcache_service->storage());
    storage->database_->db_connection();
    storage->disk_cache();
    message_loop_.RunAllPending();
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
  FilePath appcache_path = temp_dir_.path().Append(kTestingAppCacheDirname);

  // Create a ChromeAppCacheService and insert data into it
  scoped_refptr<ChromeAppCacheService> appcache_service =
      CreateAppCacheService(appcache_path, true);
  ASSERT_TRUE(file_util::PathExists(appcache_path));
  ASSERT_TRUE(file_util::PathExists(appcache_path.AppendASCII("Index")));
  InsertDataIntoAppCache(appcache_service);

  // Test: delete the ChromeAppCacheService
  appcache_service = NULL;
  message_loop_.RunAllPending();

  // Recreate the appcache (for reading the data back)
  appcache_service = CreateAppCacheService(appcache_path, false);

  // The directory is still there
  ASSERT_TRUE(file_util::PathExists(appcache_path));

  // The appcache data is also there, except the session-only origin.
  AppCacheTestHelper appcache_helper;
  std::set<GURL> origins;
  appcache_helper.GetOriginsWithCaches(appcache_service, &origins);
  EXPECT_EQ(2UL, origins.size());
  EXPECT_TRUE(origins.find(kProtectedManifestURL.GetOrigin()) != origins.end());
  EXPECT_TRUE(origins.find(kNormalManifestURL.GetOrigin()) != origins.end());
  EXPECT_TRUE(origins.find(kSessionOnlyManifestURL.GetOrigin()) ==
              origins.end());

  // Delete and let cleanup tasks run prior to returning.
  appcache_service = NULL;
  message_loop_.RunAllPending();
}

TEST_F(ChromeAppCacheServiceTest, SaveSessionState) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  FilePath appcache_path = temp_dir_.path().Append(kTestingAppCacheDirname);

  // Create a ChromeAppCacheService and insert data into it
  scoped_refptr<ChromeAppCacheService> appcache_service =
      CreateAppCacheService(appcache_path, true);
  ASSERT_TRUE(file_util::PathExists(appcache_path));
  ASSERT_TRUE(file_util::PathExists(appcache_path.AppendASCII("Index")));
  InsertDataIntoAppCache(appcache_service);

  // Save session state. This should bypass the destruction-time deletion.
  appcache_service->set_force_keep_session_state();

  // Test: delete the ChromeAppCacheService
  appcache_service = NULL;
  message_loop_.RunAllPending();

  // Recreate the appcache (for reading the data back)
  appcache_service = CreateAppCacheService(appcache_path, false);

  // The directory is still there
  ASSERT_TRUE(file_util::PathExists(appcache_path));

  // No appcache data was deleted.
  AppCacheTestHelper appcache_helper;
  std::set<GURL> origins;
  appcache_helper.GetOriginsWithCaches(appcache_service, &origins);
  EXPECT_EQ(3UL, origins.size());
  EXPECT_TRUE(origins.find(kProtectedManifestURL.GetOrigin()) != origins.end());
  EXPECT_TRUE(origins.find(kNormalManifestURL.GetOrigin()) != origins.end());
  EXPECT_TRUE(origins.find(kSessionOnlyManifestURL.GetOrigin()) !=
              origins.end());

  // Delete and let cleanup tasks run prior to returning.
  appcache_service = NULL;
  message_loop_.RunAllPending();
}

}  // namespace appcache
