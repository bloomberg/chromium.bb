// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_temp_dir.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/in_process_webkit/indexed_db_context_impl.h"
#include "content/browser/in_process_webkit/indexed_db_quota_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/database/database_util.h"

using content::BrowserContext;
using content::BrowserThread;

// Declared to shorten the line lengths.
static const quota::StorageType kTemp = quota::kStorageTypeTemporary;
static const quota::StorageType kPerm = quota::kStorageTypePersistent;

using namespace webkit_database;
using content::BrowserThreadImpl;

// Base class for our test fixtures.
class IndexedDBQuotaClientTest : public testing::Test {
 public:
  const GURL kOriginA;
  const GURL kOriginB;
  const GURL kOriginOther;

  IndexedDBQuotaClientTest()
      : kOriginA("http://host"),
        kOriginB("http://host:8000"),
        kOriginOther("http://other"),
        usage_(0),
        weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        message_loop_(MessageLoop::TYPE_IO),
        db_thread_(BrowserThread::DB, &message_loop_),
        webkit_thread_(BrowserThread::WEBKIT_DEPRECATED, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        file_user_blocking_thread_(
            BrowserThread::FILE_USER_BLOCKING, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {
    browser_context_.reset(new content::TestBrowserContext());
    idb_context_ = static_cast<IndexedDBContextImpl*>(
        BrowserContext::GetDefaultStoragePartition(browser_context_.get())->
            GetIndexedDBContext());
    message_loop_.RunAllPending();
    setup_temp_dir();
  }
  void setup_temp_dir() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    FilePath indexeddb_dir = temp_dir_.path().Append(
        IndexedDBContextImpl::kIndexedDBDirectory);
    ASSERT_TRUE(file_util::CreateDirectory(indexeddb_dir));
    idb_context()->set_data_path_for_testing(indexeddb_dir);
  }

  ~IndexedDBQuotaClientTest() {
    // IndexedDBContext needs to be destructed on
    // BrowserThread::WEBKIT_DEPRECATED, which is also a member variable of this
    // class.  Cause IndexedDBContext's destruction now to ensure that it
    // doesn't outlive BrowserThread::WEBKIT_DEPRECATED.
    idb_context_ = NULL;
    browser_context_.reset();
    MessageLoop::current()->RunAllPending();
  }

  int64 GetOriginUsage(
      quota::QuotaClient* client,
      const GURL& origin,
      quota::StorageType type) {
    usage_ = -1;
    client->GetOriginUsage(
        origin, type,
        base::Bind(&IndexedDBQuotaClientTest::OnGetOriginUsageComplete,
                   weak_factory_.GetWeakPtr()));
    MessageLoop::current()->RunAllPending();
    EXPECT_GT(usage_, -1);
    return usage_;
  }

  const std::set<GURL>& GetOriginsForType(
      quota::QuotaClient* client,
      quota::StorageType type) {
    origins_.clear();
    type_ = quota::kStorageTypeTemporary;
    client->GetOriginsForType(
        type,
        base::Bind(&IndexedDBQuotaClientTest::OnGetOriginsComplete,
                   weak_factory_.GetWeakPtr()));
    MessageLoop::current()->RunAllPending();
    return origins_;
  }

  const std::set<GURL>& GetOriginsForHost(
      quota::QuotaClient* client,
      quota::StorageType type,
      const std::string& host) {
    origins_.clear();
    type_ = quota::kStorageTypeTemporary;
    client->GetOriginsForHost(
        type, host,
        base::Bind(&IndexedDBQuotaClientTest::OnGetOriginsComplete,
                   weak_factory_.GetWeakPtr()));
    MessageLoop::current()->RunAllPending();
    return origins_;
  }

  quota::QuotaStatusCode DeleteOrigin(quota::QuotaClient* client,
                                      const GURL& origin_url) {
    delete_status_ = quota::kQuotaStatusUnknown;
    client->DeleteOriginData(
        origin_url, kTemp,
        base::Bind(&IndexedDBQuotaClientTest::OnDeleteOriginComplete,
                   weak_factory_.GetWeakPtr()));
    MessageLoop::current()->RunAllPending();
    return delete_status_;
  }

  IndexedDBContextImpl* idb_context() { return idb_context_.get(); }

  void SetFileSizeTo(const FilePath& path, int size) {
    std::string junk(size, 'a');
    ASSERT_EQ(size, file_util::WriteFile(path, junk.c_str(), size));
  }

  void AddFakeIndexedDB(const GURL& origin, int size) {
    FilePath file_path_origin = idb_context()->GetFilePathForTesting(
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
    type_ = type;
  }

  void OnDeleteOriginComplete(quota::QuotaStatusCode code) {
    delete_status_ = code;
  }

  ScopedTempDir temp_dir_;
  int64 usage_;
  std::set<GURL> origins_;
  quota::StorageType type_;
  scoped_refptr<IndexedDBContextImpl> idb_context_;
  base::WeakPtrFactory<IndexedDBQuotaClientTest> weak_factory_;
  MessageLoop message_loop_;
  BrowserThreadImpl db_thread_;
  BrowserThreadImpl webkit_thread_;
  BrowserThreadImpl file_thread_;
  BrowserThreadImpl file_user_blocking_thread_;
  BrowserThreadImpl io_thread_;
  scoped_ptr<content::TestBrowserContext> browser_context_;
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
