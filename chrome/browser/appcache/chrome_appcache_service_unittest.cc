// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/appcache/chrome_appcache_service.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/thread_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache_storage_impl.h"

namespace appcache {

class ChromeAppCacheServiceTest : public TestingBrowserProcessTest {
 public:
  ChromeAppCacheServiceTest()
      : message_loop_(MessageLoop::TYPE_IO),
        db_thread_(BrowserThread::DB, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        cache_thread_(BrowserThread::CACHE, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {
  }

 protected:
  MessageLoop message_loop_;
  ScopedTempDir temp_dir_;

 private:
  BrowserThread db_thread_;
  BrowserThread file_thread_;
  BrowserThread cache_thread_;
  BrowserThread io_thread_;
};

TEST_F(ChromeAppCacheServiceTest, KeepOnDestruction) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  FilePath appcache_path = temp_dir_.path().Append(chrome::kAppCacheDirname);
  scoped_refptr<ChromeAppCacheService> appcache_service =
      new ChromeAppCacheService;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(appcache_service.get(),
                        &ChromeAppCacheService::InitializeOnIOThread,
                        temp_dir_.path(), false,
                        scoped_refptr<HostContentSettingsMap>(NULL),
                        false));
  // Make the steps needed to initialize the storage of AppCache data.
  message_loop_.RunAllPending();
  appcache::AppCacheStorageImpl* storage =
      static_cast<appcache::AppCacheStorageImpl*>(appcache_service->storage());
  ASSERT_TRUE(storage->database_->db_connection());
  ASSERT_EQ(1, storage->NewCacheId());
  storage->disk_cache();
  message_loop_.RunAllPending();

  ASSERT_TRUE(file_util::PathExists(appcache_path));
  ASSERT_TRUE(file_util::PathExists(appcache_path.AppendASCII("Index")));

  appcache_service = NULL;
  message_loop_.RunAllPending();

  ASSERT_TRUE(file_util::PathExists(appcache_path));
}

TEST_F(ChromeAppCacheServiceTest, RemoveOnDestruction) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  FilePath appcache_path = temp_dir_.path().Append(chrome::kAppCacheDirname);
  scoped_refptr<ChromeAppCacheService> appcache_service =
      new ChromeAppCacheService;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(appcache_service.get(),
                        &ChromeAppCacheService::InitializeOnIOThread,
                        temp_dir_.path(), false,
                        scoped_refptr<HostContentSettingsMap>(NULL),
                        true));
  // Make the steps needed to initialize the storage of AppCache data.
  message_loop_.RunAllPending();
  appcache::AppCacheStorageImpl* storage =
      static_cast<appcache::AppCacheStorageImpl*>(appcache_service->storage());
  ASSERT_TRUE(storage->database_->db_connection());
  ASSERT_EQ(1, storage->NewCacheId());
  storage->disk_cache();
  message_loop_.RunAllPending();

  ASSERT_TRUE(file_util::PathExists(appcache_path));
  ASSERT_TRUE(file_util::PathExists(appcache_path.AppendASCII("Index")));

  appcache_service = NULL;
  message_loop_.RunAllPending();

  ASSERT_FALSE(file_util::PathExists(appcache_path));
}

}  // namespace appcache
