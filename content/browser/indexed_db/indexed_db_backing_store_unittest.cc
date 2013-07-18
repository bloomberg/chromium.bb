// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_backing_store.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebIDBTypes.h"
#include "url/gurl.h"
#include "webkit/common/database/database_identifier.h"

namespace content {

namespace {

class IndexedDBBackingStoreTest : public testing::Test {
 public:
  IndexedDBBackingStoreTest() {}
  virtual void SetUp() {
    std::string file_identifier;
    backing_store_ = IndexedDBBackingStore::OpenInMemory(file_identifier);

    // useful keys and values during tests
    m_value1 = "value1";
    m_value2 = "value2";
    m_value3 = "value3";
    m_key1 = IndexedDBKey(99, WebKit::WebIDBKeyTypeNumber);
    m_key2 = IndexedDBKey(ASCIIToUTF16("key2"));
    m_key3 = IndexedDBKey(ASCIIToUTF16("key3"));
  }

 protected:
  scoped_refptr<IndexedDBBackingStore> backing_store_;

  // Sample keys and values that are consistent.
  IndexedDBKey m_key1;
  IndexedDBKey m_key2;
  IndexedDBKey m_key3;
  std::string m_value1;
  std::string m_value2;
  std::string m_value3;
};

TEST_F(IndexedDBBackingStoreTest, PutGetConsistency) {
  {
    IndexedDBBackingStore::Transaction transaction1(backing_store_);
    transaction1.Begin();
    IndexedDBBackingStore::RecordIdentifier record;
    bool ok = backing_store_->PutRecord(
        &transaction1, 1, 1, m_key1, m_value1, &record);
    EXPECT_TRUE(ok);
    transaction1.Commit();
  }

  {
    IndexedDBBackingStore::Transaction transaction2(backing_store_);
    transaction2.Begin();
    std::string result_value;
    bool ok =
        backing_store_->GetRecord(&transaction2, 1, 1, m_key1, &result_value);
    transaction2.Commit();
    EXPECT_TRUE(ok);
    EXPECT_EQ(m_value1, result_value);
  }
}

// Make sure that using very high ( more than 32 bit ) values for database_id
// and object_store_id still work.
TEST_F(IndexedDBBackingStoreTest, HighIds) {
  const int64 high_database_id = 1ULL << 35;
  const int64 high_object_store_id = 1ULL << 39;
  // index_ids are capped at 32 bits for storage purposes.
  const int64 high_index_id = 1ULL << 29;

  const int64 invalid_high_index_id = 1ULL << 37;

  const IndexedDBKey& index_key = m_key2;
  std::string index_key_raw;
  EncodeIDBKey(index_key, &index_key_raw);
  {
    IndexedDBBackingStore::Transaction transaction1(backing_store_);
    transaction1.Begin();
    IndexedDBBackingStore::RecordIdentifier record;
    bool ok = backing_store_->PutRecord(&transaction1,
                                        high_database_id,
                                        high_object_store_id,
                                        m_key1,
                                        m_value1,
                                        &record);
    EXPECT_TRUE(ok);

    ok = backing_store_->PutIndexDataForRecord(&transaction1,
                                               high_database_id,
                                               high_object_store_id,
                                               invalid_high_index_id,
                                               index_key,
                                               record);
    EXPECT_FALSE(ok);

    ok = backing_store_->PutIndexDataForRecord(&transaction1,
                                               high_database_id,
                                               high_object_store_id,
                                               high_index_id,
                                               index_key,
                                               record);
    EXPECT_TRUE(ok);

    ok = transaction1.Commit();
    EXPECT_TRUE(ok);
  }

  {
    IndexedDBBackingStore::Transaction transaction2(backing_store_);
    transaction2.Begin();
    std::string result_value;
    bool ok = backing_store_->GetRecord(&transaction2,
                                        high_database_id,
                                        high_object_store_id,
                                        m_key1,
                                        &result_value);
    EXPECT_TRUE(ok);
    EXPECT_EQ(m_value1, result_value);

    scoped_ptr<IndexedDBKey> new_primary_key;
    ok = backing_store_->GetPrimaryKeyViaIndex(&transaction2,
                                               high_database_id,
                                               high_object_store_id,
                                               invalid_high_index_id,
                                               index_key,
                                               &new_primary_key);
    EXPECT_FALSE(ok);

    ok = backing_store_->GetPrimaryKeyViaIndex(&transaction2,
                                               high_database_id,
                                               high_object_store_id,
                                               high_index_id,
                                               index_key,
                                               &new_primary_key);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(new_primary_key->IsEqual(m_key1));

    ok = transaction2.Commit();
    EXPECT_TRUE(ok);
  }
}

// Make sure that other invalid ids do not crash.
TEST_F(IndexedDBBackingStoreTest, InvalidIds) {
  // valid ids for use when testing invalid ids
  const int64 database_id = 1;
  const int64 object_store_id = 1;
  const int64 index_id = kMinimumIndexId;
  const int64 invalid_low_index_id = 19;  // index_ids must be > kMinimumIndexId

  std::string result_value;

  IndexedDBBackingStore::Transaction transaction1(backing_store_);
  transaction1.Begin();

  IndexedDBBackingStore::RecordIdentifier record;
  bool ok = backing_store_->PutRecord(&transaction1,
                                      database_id,
                                      KeyPrefix::kInvalidId,
                                      m_key1,
                                      m_value1,
                                      &record);
  EXPECT_FALSE(ok);
  ok = backing_store_->PutRecord(
      &transaction1, database_id, 0, m_key1, m_value1, &record);
  EXPECT_FALSE(ok);
  ok = backing_store_->PutRecord(&transaction1,
                                 KeyPrefix::kInvalidId,
                                 object_store_id,
                                 m_key1,
                                 m_value1,
                                 &record);
  EXPECT_FALSE(ok);
  ok = backing_store_->PutRecord(
      &transaction1, 0, object_store_id, m_key1, m_value1, &record);
  EXPECT_FALSE(ok);

  ok = backing_store_->GetRecord(
      &transaction1, database_id, KeyPrefix::kInvalidId, m_key1, &result_value);
  EXPECT_FALSE(ok);
  ok = backing_store_->GetRecord(
      &transaction1, database_id, 0, m_key1, &result_value);
  EXPECT_FALSE(ok);
  ok = backing_store_->GetRecord(&transaction1,
                                 KeyPrefix::kInvalidId,
                                 object_store_id,
                                 m_key1,
                                 &result_value);
  EXPECT_FALSE(ok);
  ok = backing_store_->GetRecord(
      &transaction1, 0, object_store_id, m_key1, &result_value);
  EXPECT_FALSE(ok);

  scoped_ptr<IndexedDBKey> new_primary_key;
  ok = backing_store_->GetPrimaryKeyViaIndex(&transaction1,
                                             database_id,
                                             object_store_id,
                                             KeyPrefix::kInvalidId,
                                             m_key1,
                                             &new_primary_key);
  EXPECT_FALSE(ok);
  ok = backing_store_->GetPrimaryKeyViaIndex(&transaction1,
                                             database_id,
                                             object_store_id,
                                             invalid_low_index_id,
                                             m_key1,
                                             &new_primary_key);
  EXPECT_FALSE(ok);
  ok = backing_store_->GetPrimaryKeyViaIndex(
      &transaction1, database_id, object_store_id, 0, m_key1, &new_primary_key);
  EXPECT_FALSE(ok);

  ok = backing_store_->GetPrimaryKeyViaIndex(&transaction1,
                                             KeyPrefix::kInvalidId,
                                             object_store_id,
                                             index_id,
                                             m_key1,
                                             &new_primary_key);
  EXPECT_FALSE(ok);
  ok = backing_store_->GetPrimaryKeyViaIndex(&transaction1,
                                             database_id,
                                             KeyPrefix::kInvalidId,
                                             index_id,
                                             m_key1,
                                             &new_primary_key);
  EXPECT_FALSE(ok);
}

TEST_F(IndexedDBBackingStoreTest, CreateDatabase) {
  const string16 database_name(ASCIIToUTF16("db1"));
  int64 database_id;
  const string16 version(ASCIIToUTF16("old_string_version"));
  const int64 int_version = 9;

  const int64 object_store_id = 99;
  const string16 object_store_name(ASCIIToUTF16("object_store1"));
  const bool auto_increment = true;
  const IndexedDBKeyPath object_store_key_path(
      ASCIIToUTF16("object_store_key"));

  const int64 index_id = 999;
  const string16 index_name(ASCIIToUTF16("index1"));
  const bool unique = true;
  const bool multi_entry = true;
  const IndexedDBKeyPath index_key_path(ASCIIToUTF16("index_key"));

  {
    bool ok = backing_store_->CreateIDBDatabaseMetaData(
        database_name, version, int_version, &database_id);
    EXPECT_TRUE(ok);
    EXPECT_GT(database_id, 0);

    IndexedDBBackingStore::Transaction transaction(backing_store_);
    transaction.Begin();

    ok = backing_store_->CreateObjectStore(&transaction,
                                           database_id,
                                           object_store_id,
                                           object_store_name,
                                           object_store_key_path,
                                           auto_increment);
    EXPECT_TRUE(ok);

    ok = backing_store_->CreateIndex(&transaction,
                                     database_id,
                                     object_store_id,
                                     index_id,
                                     index_name,
                                     index_key_path,
                                     unique,
                                     multi_entry);
    EXPECT_TRUE(ok);

    ok = transaction.Commit();
    EXPECT_TRUE(ok);
  }

  {
    IndexedDBDatabaseMetadata database;
    bool found;
    bool ok = backing_store_->GetIDBDatabaseMetaData(
        database_name, &database, &found);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(found);

    // database.name is not filled in by the implementation.
    EXPECT_EQ(version, database.version);
    EXPECT_EQ(int_version, database.int_version);
    EXPECT_EQ(database_id, database.id);

    ok = backing_store_->GetObjectStores(database.id, &database.object_stores);
    EXPECT_TRUE(ok);

    EXPECT_EQ(1UL, database.object_stores.size());
    IndexedDBObjectStoreMetadata object_store =
        database.object_stores[object_store_id];
    EXPECT_EQ(object_store_name, object_store.name);
    EXPECT_EQ(object_store_key_path, object_store.key_path);
    EXPECT_EQ(auto_increment, object_store.auto_increment);

    EXPECT_EQ(1UL, object_store.indexes.size());
    IndexedDBIndexMetadata index = object_store.indexes[index_id];
    EXPECT_EQ(index_name, index.name);
    EXPECT_EQ(index_key_path, index.key_path);
    EXPECT_EQ(unique, index.unique);
    EXPECT_EQ(multi_entry, index.multi_entry);
  }
}

class MockIDBFactory : public IndexedDBFactory {
 public:
  scoped_refptr<IndexedDBBackingStore> TestOpenBackingStore(
      const GURL& origin,
      const base::FilePath& data_directory) {
    WebKit::WebIDBCallbacks::DataLoss data_loss =
        WebKit::WebIDBCallbacks::DataLossNone;
    scoped_refptr<IndexedDBBackingStore> backing_store =
        OpenBackingStore(webkit_database::GetIdentifierFromOrigin(origin),
                         data_directory,
                         &data_loss);
    EXPECT_EQ(WebKit::WebIDBCallbacks::DataLossNone, data_loss);
    return backing_store;
  }

 private:
  virtual ~MockIDBFactory() {}
};

TEST(IndexedDBFactoryTest, BackingStoreLifetime) {
  GURL origin1("http://localhost:81");
  GURL origin2("http://localhost:82");

  scoped_refptr<MockIDBFactory> factory = new MockIDBFactory();

  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  scoped_refptr<IndexedDBBackingStore> disk_store1 =
      factory->TestOpenBackingStore(origin1, temp_directory.path());
  EXPECT_TRUE(disk_store1->HasOneRef());

  scoped_refptr<IndexedDBBackingStore> disk_store2 =
      factory->TestOpenBackingStore(origin1, temp_directory.path());
  EXPECT_EQ(disk_store1.get(), disk_store2.get());
  EXPECT_FALSE(disk_store2->HasOneRef());

  scoped_refptr<IndexedDBBackingStore> disk_store3 =
      factory->TestOpenBackingStore(origin2, temp_directory.path());
  EXPECT_TRUE(disk_store3->HasOneRef());
  EXPECT_FALSE(disk_store1->HasOneRef());

  disk_store2 = NULL;
  EXPECT_TRUE(disk_store1->HasOneRef());
}

TEST(IndexedDBFactoryTest, MemoryBackingStoreLifetime) {
  GURL origin1("http://localhost:81");
  GURL origin2("http://localhost:82");

  scoped_refptr<MockIDBFactory> factory = new MockIDBFactory();
  scoped_refptr<IndexedDBBackingStore> mem_store1 =
      factory->TestOpenBackingStore(origin1, base::FilePath());
  EXPECT_FALSE(mem_store1->HasOneRef());  // mem_store1 and factory

  scoped_refptr<IndexedDBBackingStore> mem_store2 =
      factory->TestOpenBackingStore(origin1, base::FilePath());
  EXPECT_EQ(mem_store1.get(), mem_store2.get());
  EXPECT_FALSE(mem_store1->HasOneRef());  // mem_store1, 2 and factory
  EXPECT_FALSE(mem_store2->HasOneRef());  // mem_store1, 2 and factory

  scoped_refptr<IndexedDBBackingStore> mem_store3 =
      factory->TestOpenBackingStore(origin2, base::FilePath());
  EXPECT_FALSE(mem_store1->HasOneRef());  // mem_store1, 2 and factory
  EXPECT_FALSE(mem_store3->HasOneRef());  // mem_store3 and factory

  factory = NULL;
  EXPECT_FALSE(mem_store1->HasOneRef());  // mem_store1 and 2
  EXPECT_FALSE(mem_store2->HasOneRef());  // mem_store1 and 2
  EXPECT_TRUE(mem_store3->HasOneRef());

  mem_store2 = NULL;
  EXPECT_TRUE(mem_store1->HasOneRef());
}

TEST(IndexedDBFactoryTest, RejectLongOrigins) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  const base::FilePath base_path = temp_directory.path();
  scoped_refptr<MockIDBFactory> factory = new MockIDBFactory();

  int limit = file_util::GetMaximumPathComponentLength(base_path);
  EXPECT_GT(limit, 0);

  std::string origin(limit + 1, 'x');
  GURL too_long_origin("http://" + origin + ":81/");
  scoped_refptr<IndexedDBBackingStore> diskStore1 =
      factory->TestOpenBackingStore(too_long_origin, base_path);
  EXPECT_FALSE(diskStore1);

  GURL ok_origin("http://someorigin.com:82/");
  scoped_refptr<IndexedDBBackingStore> diskStore2 =
      factory->TestOpenBackingStore(ok_origin, base_path);
  EXPECT_TRUE(diskStore2);
}

}  // namespace

}  // namespace content
