// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/browser/value_store/value_store_frontend.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class ValueStoreFrontendTest : public testing::Test {
 public:
  ValueStoreFrontendTest()
      : ui_thread_(BrowserThread::UI, base::MessageLoop::current()),
        file_thread_(BrowserThread::FILE, base::MessageLoop::current()) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    base::FilePath src_db(test_data_dir.AppendASCII("value_store_db"));
    db_path_ = temp_dir_.path().AppendASCII("temp_db");
    base::CopyDirectory(src_db, db_path_, true);

    ResetStorage();
  }

  virtual void TearDown() {
    base::MessageLoop::current()->RunUntilIdle();  // wait for storage to delete
    storage_.reset();
  }

  // Reset the value store, reloading the DB from disk.
  void ResetStorage() {
    storage_.reset(new ValueStoreFrontend(db_path_));
  }

  bool Get(const std::string& key, scoped_ptr<base::Value>* output) {
    storage_->Get(key, base::Bind(&ValueStoreFrontendTest::GetAndWait,
                                  base::Unretained(this), output));
    base::MessageLoop::current()->Run();  // wait for GetAndWait
    return !!output->get();
  }

 protected:
  void GetAndWait(scoped_ptr<base::Value>* output,
                  scoped_ptr<base::Value> result) {
    *output = result.Pass();
    base::MessageLoop::current()->Quit();
  }

  scoped_ptr<ValueStoreFrontend> storage_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
};

TEST_F(ValueStoreFrontendTest, GetExistingData) {
  scoped_ptr<base::Value> value;
  ASSERT_FALSE(Get("key0", &value));

  // Test existing keys in the DB.
  {
    ASSERT_TRUE(Get("key1", &value));
    std::string result;
    ASSERT_TRUE(value->GetAsString(&result));
    EXPECT_EQ("value1", result);
  }

  {
    ASSERT_TRUE(Get("key2", &value));
    int result;
    ASSERT_TRUE(value->GetAsInteger(&result));
    EXPECT_EQ(2, result);
  }
}

TEST_F(ValueStoreFrontendTest, ChangesPersistAfterReload) {
  storage_->Set("key0", scoped_ptr<base::Value>(new base::FundamentalValue(0)));
  storage_->Set("key1", scoped_ptr<base::Value>(new base::StringValue("new1")));
  storage_->Remove("key2");

  // Reload the DB and test our changes.
  ResetStorage();

  scoped_ptr<base::Value> value;
  {
    ASSERT_TRUE(Get("key0", &value));
    int result;
    ASSERT_TRUE(value->GetAsInteger(&result));
    EXPECT_EQ(0, result);
  }

  {
    ASSERT_TRUE(Get("key1", &value));
    std::string result;
    ASSERT_TRUE(value->GetAsString(&result));
    EXPECT_EQ("new1", result);
  }

  ASSERT_FALSE(Get("key2", &value));
}
