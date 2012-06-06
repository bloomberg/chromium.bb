// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/value_store/caching_value_store.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

// Test suite for CachingValueStore, using a test database with a few simple
// entries.
class CachingValueStoreTest
    : public testing::Test,
      public CachingValueStore::Observer {
 public:
  CachingValueStoreTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()),
        file_thread_(BrowserThread::FILE, MessageLoop::current()) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    FilePath src_db(test_data_dir.AppendASCII("value_store_db"));
    db_path_ = temp_dir_.path().AppendASCII("temp_db");
    file_util::CopyDirectory(src_db, db_path_, true);

    ResetStorage();
  }

  virtual void TearDown() {
    MessageLoop::current()->RunAllPending();  // wait for storage to delete
    storage_->RemoveObserver(this);
    storage_.reset();
  }

  // CachingValueStore::Observer
  virtual void OnInitializationComplete() {
    MessageLoop::current()->Quit();
  }

  // Reset the value store, reloading the DB from disk.
  void ResetStorage() {
    if (storage_.get())
      storage_->RemoveObserver(this);
    storage_.reset(new CachingValueStore(db_path_));
    storage_->AddObserver(this);
    MessageLoop::current()->Run();  // wait for OnInitializationComplete
  }

 protected:
  scoped_ptr<CachingValueStore> storage_;
  ScopedTempDir temp_dir_;
  FilePath db_path_;
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
};

TEST_F(CachingValueStoreTest, GetExistingData) {
  const base::Value* value = NULL;
  ASSERT_FALSE(storage_->Get("key0", &value));

  // Test existing keys in the DB.
  {
    ASSERT_TRUE(storage_->Get("key1", &value));
    std::string result;
    ASSERT_TRUE(value->GetAsString(&result));
    EXPECT_EQ("value1", result);
  }

  {
    ASSERT_TRUE(storage_->Get("key2", &value));
    int result;
    ASSERT_TRUE(value->GetAsInteger(&result));
    EXPECT_EQ(2, result);
  }
}

TEST_F(CachingValueStoreTest, ChangesPersistAfterReload) {
  storage_->Set("key0", base::Value::CreateIntegerValue(0));
  storage_->Set("key1", base::Value::CreateStringValue("new1"));
  storage_->Remove("key2");

  // Reload the DB and test our changes.
  ResetStorage();

  const base::Value* value = NULL;
  {
    ASSERT_TRUE(storage_->Get("key0", &value));
    int result;
    ASSERT_TRUE(value->GetAsInteger(&result));
    EXPECT_EQ(0, result);
  }

  {
    ASSERT_TRUE(storage_->Get("key1", &value));
    std::string result;
    ASSERT_TRUE(value->GetAsString(&result));
    EXPECT_EQ("new1", result);
  }

  ASSERT_FALSE(storage_->Get("key2", &value));
}
