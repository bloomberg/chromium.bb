// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabase.h"
#include "webkit/database/database_util.h"
#include "webkit/quota/mock_special_storage_policy.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/special_storage_policy.h"

using webkit_database::DatabaseUtil;

namespace content {

class IndexedDBTest : public testing::Test {
 public:
  IndexedDBTest()
      : message_loop_(MessageLoop::TYPE_IO),
        webkit_thread_(BrowserThread::WEBKIT_DEPRECATED, &message_loop_),
        file_thread_(BrowserThread::FILE_USER_BLOCKING, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {
  }

 protected:
  MessageLoop message_loop_;

 private:
  BrowserThreadImpl webkit_thread_;
  BrowserThreadImpl file_thread_;
  BrowserThreadImpl io_thread_;
};

TEST_F(IndexedDBTest, ClearSessionOnlyDatabases) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath normal_path;
  base::FilePath session_only_path;

  // Create the scope which will ensure we run the destructor of the webkit
  // context which should trigger the clean up.
  {
    TestBrowserContext browser_context;

    const GURL kNormalOrigin("http://normal/");
    const GURL kSessionOnlyOrigin("http://session-only/");
    scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy =
        new quota::MockSpecialStoragePolicy;
    special_storage_policy->AddSessionOnly(kSessionOnlyOrigin);

    // Create some indexedDB paths.
    // With the levelDB backend, these are directories.
    IndexedDBContextImpl* idb_context =
        static_cast<IndexedDBContextImpl*>(
            BrowserContext::GetDefaultStoragePartition(&browser_context)->
                GetIndexedDBContext());

    // Override the storage policy with our own.
    idb_context->special_storage_policy_ = special_storage_policy;
    idb_context->set_data_path_for_testing(temp_dir.path());

    normal_path = idb_context->GetFilePathForTesting(
        DatabaseUtil::GetOriginIdentifier(kNormalOrigin));
    session_only_path = idb_context->GetFilePathForTesting(
        DatabaseUtil::GetOriginIdentifier(kSessionOnlyOrigin));
    ASSERT_TRUE(file_util::CreateDirectory(normal_path));
    ASSERT_TRUE(file_util::CreateDirectory(session_only_path));
    message_loop_.RunUntilIdle();
  }

  message_loop_.RunUntilIdle();

  EXPECT_TRUE(file_util::DirectoryExists(normal_path));
  EXPECT_FALSE(file_util::DirectoryExists(session_only_path));
}

TEST_F(IndexedDBTest, SetForceKeepSessionState) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath normal_path;
  base::FilePath session_only_path;

  // Create the scope which will ensure we run the destructor of the webkit
  // context.
  {
    TestBrowserContext browser_context;

    const GURL kNormalOrigin("http://normal/");
    const GURL kSessionOnlyOrigin("http://session-only/");
    scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy =
        new quota::MockSpecialStoragePolicy;
    special_storage_policy->AddSessionOnly(kSessionOnlyOrigin);

    // Create some indexedDB paths.
    // With the levelDB backend, these are directories.
    IndexedDBContextImpl* idb_context =
        static_cast<IndexedDBContextImpl*>(
            BrowserContext::GetDefaultStoragePartition(&browser_context)->
                GetIndexedDBContext());

    // Override the storage policy with our own.
    idb_context->special_storage_policy_ = special_storage_policy;
    idb_context->set_data_path_for_testing(temp_dir.path());

    // Save session state. This should bypass the destruction-time deletion.
    idb_context->SetForceKeepSessionState();

    normal_path = idb_context->GetFilePathForTesting(
        DatabaseUtil::GetOriginIdentifier(kNormalOrigin));
    session_only_path = idb_context->GetFilePathForTesting(
        DatabaseUtil::GetOriginIdentifier(kSessionOnlyOrigin));
    ASSERT_TRUE(file_util::CreateDirectory(normal_path));
    ASSERT_TRUE(file_util::CreateDirectory(session_only_path));
    message_loop_.RunUntilIdle();
  }

  // Make sure we wait until the destructor has run.
  message_loop_.RunUntilIdle();

  // No data was cleared because of SetForceKeepSessionState.
  EXPECT_TRUE(file_util::DirectoryExists(normal_path));
  EXPECT_TRUE(file_util::DirectoryExists(session_only_path));
}

class MockWebIDBDatabase : public WebKit::WebIDBDatabase
{
 public:
  MockWebIDBDatabase(bool expect_force_close)
      : expect_force_close_(expect_force_close),
        force_close_called_(false) {}

  virtual ~MockWebIDBDatabase()
  {
    EXPECT_TRUE(force_close_called_ == expect_force_close_);
  }

  virtual void forceClose()
  {
    ASSERT_TRUE(expect_force_close_);
    force_close_called_ = true;
  }

 private:
  bool expect_force_close_;
  bool force_close_called_;
};


TEST_F(IndexedDBTest, ForceCloseOpenDatabasesOnDelete) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath test_path;

  // Create the scope which will ensure we run the destructor of the webkit
  // context.
  {
    TestBrowserContext browser_context;

    const GURL kTestOrigin("http://test/");

    IndexedDBContextImpl* idb_context =
        static_cast<IndexedDBContextImpl*>(
            BrowserContext::GetDefaultStoragePartition(&browser_context)->
                GetIndexedDBContext());

    idb_context->quota_manager_proxy_ = NULL;
    idb_context->set_data_path_for_testing(temp_dir.path());

    test_path = idb_context->GetFilePathForTesting(
        DatabaseUtil::GetOriginIdentifier(kTestOrigin));
    ASSERT_TRUE(file_util::CreateDirectory(test_path));

    const bool kExpectForceClose = true;

    MockWebIDBDatabase connection1(kExpectForceClose);
    idb_context->ConnectionOpened(kTestOrigin, &connection1);

    MockWebIDBDatabase connection2(!kExpectForceClose);
    idb_context->ConnectionOpened(kTestOrigin, &connection2);
    idb_context->ConnectionClosed(kTestOrigin, &connection2);

    idb_context->DeleteForOrigin(kTestOrigin);

    message_loop_.RunUntilIdle();
  }

  // Make sure we wait until the destructor has run.
  message_loop_.RunUntilIdle();

  EXPECT_FALSE(file_util::DirectoryExists(test_path));
}

}  // namespace content
