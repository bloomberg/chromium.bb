// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/in_process_webkit/indexed_db_context.h"
#include "content/browser/in_process_webkit/indexed_db_quota_client.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/database/database_util.h"

// Declared to shorten the line lengths.
static const quota::StorageType kTemp = quota::kStorageTypeTemporary;
static const quota::StorageType kPerm = quota::kStorageTypePersistent;

using namespace webkit_database;

// Base class for our test fixtures.
class IndexedDBQuotaClientTest : public testing::Test {
 public:
  const GURL kOriginA;
  const GURL kOriginB;

  IndexedDBQuotaClientTest()
      : kOriginA("http://host"),
        kOriginB("http://host:8000"),
        usage_(0),
        callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        message_loop_(MessageLoop::TYPE_IO) {
    TestingProfile profile;
    idb_context_ = new IndexedDBContext(profile.GetWebKitContext(), NULL,
                                        NULL, NULL);
  }

  int64 GetOriginUsage(
      quota::QuotaClient* client,
      const GURL& origin,
      quota::StorageType type) {
    usage_ = -1;
    client->GetOriginUsage(origin, type,
        callback_factory_.NewCallback(
            &IndexedDBQuotaClientTest::OnGetOriginUsageComplete));
    MessageLoop::current()->RunAllPending();
    EXPECT_GT(usage_, -1);
    return usage_;
  }

  IndexedDBContext* idb_context() { return idb_context_.get(); }

  void SetFileSizeTo(const FilePath& path, int size) {
    std::string junk(size, 'a');
    ASSERT_EQ(size, file_util::WriteFile(path, junk.c_str(), size));
  }


 private:
  void OnGetOriginUsageComplete(int64 usage) {
    usage_ = usage;
  }

  int64 usage_;
  scoped_refptr<IndexedDBContext> idb_context_;
  base::ScopedCallbackFactory<IndexedDBQuotaClientTest> callback_factory_;
  MessageLoop message_loop_;
};


TEST_F(IndexedDBQuotaClientTest, GetOriginUsage) {
  IndexedDBQuotaClient client(
      base::MessageLoopProxy::CreateForCurrentThread(),
      idb_context());

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath indexeddb_dir = temp_dir.path().Append(
      IndexedDBContext::kIndexedDBDirectory);
  ASSERT_TRUE(file_util::CreateDirectory(indexeddb_dir));

  idb_context()->set_data_path(indexeddb_dir);
  FilePath file_path_origin_a = idb_context()->GetIndexedDBFilePath(
      DatabaseUtil::GetOriginIdentifier(kOriginA));
  FilePath file_path_origin_b = idb_context()->GetIndexedDBFilePath(
      DatabaseUtil::GetOriginIdentifier(kOriginB));

  SetFileSizeTo(file_path_origin_a, 6);
  SetFileSizeTo(file_path_origin_b, 3);
  EXPECT_EQ(6, GetOriginUsage(&client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginA, kPerm));
  EXPECT_EQ(3, GetOriginUsage(&client, kOriginB, kTemp));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginB, kPerm));

  SetFileSizeTo(file_path_origin_a, 1000);
  EXPECT_EQ(1000, GetOriginUsage(&client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginA, kPerm));
  EXPECT_EQ(3, GetOriginUsage(&client, kOriginB, kTemp));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginB, kPerm));
}
