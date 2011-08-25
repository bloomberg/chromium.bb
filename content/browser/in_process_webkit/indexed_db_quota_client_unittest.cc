// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "chrome/test/base/testing_browser_process_test.h"
#include "chrome/test/base/testing_profile.h"
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
class IndexedDBQuotaClientTest : public TestingBrowserProcessTest {
 public:
  const GURL kOriginA;
  const GURL kOriginB;
  const GURL kOriginOther;

  IndexedDBQuotaClientTest()
      : kOriginA("http://host"),
        kOriginB("http://host:8000"),
        kOriginOther("http://other"),
        usage_(0),
        callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        message_loop_(MessageLoop::TYPE_IO),
        webkit_thread_(BrowserThread::WEBKIT, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {
    TestingProfile profile;
    idb_context_ = profile.GetWebKitContext()->indexed_db_context();
    setup_temp_dir();
  }
  void setup_temp_dir() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    FilePath indexeddb_dir = temp_dir_.path().Append(
        IndexedDBContext::kIndexedDBDirectory);
    ASSERT_TRUE(file_util::CreateDirectory(indexeddb_dir));
    idb_context()->set_data_path(indexeddb_dir);
  }

  ~IndexedDBQuotaClientTest() {
    // IndexedDBContext needs to be destructed on BrowserThread::WEBKIT, which
    // is also a member variable of this class.  Cause IndexedDBContext's
    // destruction now to ensure that it doesn't outlive BrowserThread::WEBKIT.
    idb_context_ = NULL;
    MessageLoop::current()->RunAllPending();
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

  const std::set<GURL>& GetOriginsForType(
      quota::QuotaClient* client,
      quota::StorageType type) {
    origins_.clear();
    type_ = quota::kStorageTypeTemporary;
    client->GetOriginsForType(type,
        callback_factory_.NewCallback(
            &IndexedDBQuotaClientTest::OnGetOriginsComplete));
    MessageLoop::current()->RunAllPending();
    return origins_;
  }

  const std::set<GURL>& GetOriginsForHost(
      quota::QuotaClient* client,
      quota::StorageType type,
      const std::string& host) {
    origins_.clear();
    type_ = quota::kStorageTypeTemporary;
    client->GetOriginsForHost(type, host,
        callback_factory_.NewCallback(
            &IndexedDBQuotaClientTest::OnGetOriginsComplete));
    MessageLoop::current()->RunAllPending();
    return origins_;
  }

  quota::QuotaStatusCode DeleteOrigin(quota::QuotaClient* client,
                                      const GURL& origin_url) {
    delete_status_ = quota::kQuotaStatusUnknown;
    client->DeleteOriginData(origin_url, kTemp, callback_factory_.NewCallback(
        &IndexedDBQuotaClientTest::OnDeleteOriginComplete));
    MessageLoop::current()->RunAllPending();
    return delete_status_;
  }

  IndexedDBContext* idb_context() { return idb_context_.get(); }

  void SetFileSizeTo(const FilePath& path, int size) {
    std::string junk(size, 'a');
    ASSERT_EQ(size, file_util::WriteFile(path, junk.c_str(), size));
  }

  void AddFakeIndexedDB(const GURL& origin, int size) {
    FilePath file_path_origin = idb_context()->GetIndexedDBFilePath(
        DatabaseUtil::GetOriginIdentifier(origin));
    if (!file_util::CreateDirectory(file_path_origin)) {
      LOG(ERROR) << "failed to file_util::CreateDirectory "
                 << file_path_origin.value();
    }
    file_path_origin = file_path_origin.Append(FILE_PATH_LITERAL("fake_file"));
    SetFileSizeTo(file_path_origin, size);
    idb_context()->ResetCaches();
  }

 private:
  void OnGetOriginUsageComplete(int64 usage) {
    usage_ = usage;
  }

  void OnGetOriginsComplete(const std::set<GURL>& origins,
      quota::StorageType type) {
    origins_ = origins;
    type_ = type_;
  }

  void OnDeleteOriginComplete(quota::QuotaStatusCode code) {
    delete_status_ = code;
  }

  ScopedTempDir temp_dir_;
  int64 usage_;
  std::set<GURL> origins_;
  quota::StorageType type_;
  scoped_refptr<IndexedDBContext> idb_context_;
  base::ScopedCallbackFactory<IndexedDBQuotaClientTest> callback_factory_;
  MessageLoop message_loop_;
  BrowserThread webkit_thread_;
  BrowserThread io_thread_;
  quota::QuotaStatusCode delete_status_;
};


TEST_F(IndexedDBQuotaClientTest, GetOriginUsage) {
  IndexedDBQuotaClient client(
      base::MessageLoopProxy::current(),
      idb_context());

  AddFakeIndexedDB(kOriginA, 6);
  AddFakeIndexedDB(kOriginB, 3);
  EXPECT_EQ(6, GetOriginUsage(&client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginA, kPerm));
  EXPECT_EQ(3, GetOriginUsage(&client, kOriginB, kTemp));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginB, kPerm));

  AddFakeIndexedDB(kOriginA, 1000);
  EXPECT_EQ(1000, GetOriginUsage(&client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginA, kPerm));
  EXPECT_EQ(3, GetOriginUsage(&client, kOriginB, kTemp));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginB, kPerm));
}

TEST_F(IndexedDBQuotaClientTest, GetOriginsForHost) {
  IndexedDBQuotaClient client(
      base::MessageLoopProxy::current(),
      idb_context());

  EXPECT_EQ(kOriginA.host(), kOriginB.host());
  EXPECT_NE(kOriginA.host(), kOriginOther.host());

  std::set<GURL> origins = GetOriginsForHost(&client, kTemp, kOriginA.host());
  EXPECT_TRUE(origins.empty());

  AddFakeIndexedDB(kOriginA, 1000);
  origins = GetOriginsForHost(&client, kTemp, kOriginA.host());
  EXPECT_EQ(origins.size(), 1ul);
  EXPECT_TRUE(origins.find(kOriginA) != origins.end());

  AddFakeIndexedDB(kOriginB, 1000);
  origins = GetOriginsForHost(&client, kTemp, kOriginA.host());
  EXPECT_EQ(origins.size(), 2ul);
  EXPECT_TRUE(origins.find(kOriginA) != origins.end());
  EXPECT_TRUE(origins.find(kOriginB) != origins.end());

  EXPECT_TRUE(GetOriginsForHost(&client, kPerm, kOriginA.host()).empty());
  EXPECT_TRUE(GetOriginsForHost(&client, kTemp, kOriginOther.host()).empty());
}

TEST_F(IndexedDBQuotaClientTest, GetOriginsForType) {
  IndexedDBQuotaClient client(
      base::MessageLoopProxy::current(),
      idb_context());

  EXPECT_TRUE(GetOriginsForType(&client, kTemp).empty());
  EXPECT_TRUE(GetOriginsForType(&client, kPerm).empty());

  AddFakeIndexedDB(kOriginA, 1000);
  std::set<GURL> origins = GetOriginsForType(&client, kTemp);
  EXPECT_EQ(origins.size(), 1ul);
  EXPECT_TRUE(origins.find(kOriginA) != origins.end());

  EXPECT_TRUE(GetOriginsForType(&client, kPerm).empty());
}

TEST_F(IndexedDBQuotaClientTest, DeleteOrigin) {
  IndexedDBQuotaClient client(
      base::MessageLoopProxy::current(),
      idb_context());

  AddFakeIndexedDB(kOriginA, 1000);
  AddFakeIndexedDB(kOriginB, 50);
  EXPECT_EQ(1000, GetOriginUsage(&client, kOriginA, kTemp));
  EXPECT_EQ(50, GetOriginUsage(&client, kOriginB, kTemp));

  quota::QuotaStatusCode delete_status = DeleteOrigin(&client, kOriginA);
  EXPECT_EQ(quota::kQuotaStatusOk, delete_status);
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginA, kTemp));
  EXPECT_EQ(50, GetOriginUsage(&client, kOriginB, kTemp));
}
