// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_fetch_stores_manager.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ServiceWorkerFetchStoresManagerTest : public testing::Test {
 protected:
  ServiceWorkerFetchStoresManagerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        callback_bool_(false),
        callback_store_(0),
        callback_error_(ServiceWorkerFetchStores::FETCH_STORES_ERROR_NO_ERROR),
        origin1_("http://example1.com"),
        origin2_("http://example2.com") {}

  virtual void SetUp() OVERRIDE {
    if (MemoryOnly()) {
      stores_ = ServiceWorkerFetchStoresManager::Create(
          base::FilePath(), base::MessageLoopProxy::current());
    } else {
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
      stores_ = ServiceWorkerFetchStoresManager::Create(
          temp_dir_.path(), base::MessageLoopProxy::current());
    }
  }

  virtual bool MemoryOnly() { return false; }

  void BoolAndErrorCallback(base::RunLoop* run_loop,
                            bool value,
                            ServiceWorkerFetchStores::FetchStoresError error) {
    callback_bool_ = value;
    callback_error_ = error;
    run_loop->Quit();
  }

  void StoreAndErrorCallback(base::RunLoop* run_loop,
                             int store,
                             ServiceWorkerFetchStores::FetchStoresError error) {
    callback_store_ = store;
    callback_error_ = error;
    run_loop->Quit();
  }

  void StringsAndErrorCallback(
      base::RunLoop* run_loop,
      const std::vector<std::string>& strings,
      ServiceWorkerFetchStores::FetchStoresError error) {
    callback_strings_ = strings;
    callback_error_ = error;
    run_loop->Quit();
  }

  bool CreateStore(const GURL& origin, std::string key) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    stores_->CreateStore(
        origin,
        key,
        base::Bind(&ServiceWorkerFetchStoresManagerTest::StoreAndErrorCallback,
                   base::Unretained(this),
                   base::Unretained(loop.get())));
    loop->Run();

    bool error = callback_error_ !=
                 ServiceWorkerFetchStores::FETCH_STORES_ERROR_NO_ERROR;
    if (error)
      EXPECT_EQ(0, callback_store_);
    else
      EXPECT_LT(0, callback_store_);
    return !error;
  }

  bool Get(const GURL& origin, std::string key) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    stores_->GetStore(
        origin,
        key,
        base::Bind(&ServiceWorkerFetchStoresManagerTest::StoreAndErrorCallback,
                   base::Unretained(this),
                   base::Unretained(loop.get())));
    loop->Run();

    bool error = callback_error_ !=
                 ServiceWorkerFetchStores::FETCH_STORES_ERROR_NO_ERROR;
    if (error)
      EXPECT_EQ(0, callback_store_);
    else
      EXPECT_LT(0, callback_store_);
    return !error;
  }

  bool Has(const GURL& origin, std::string key) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    stores_->HasStore(
        origin,
        key,
        base::Bind(&ServiceWorkerFetchStoresManagerTest::BoolAndErrorCallback,
                   base::Unretained(this),
                   base::Unretained(loop.get())));
    loop->Run();

    return callback_bool_;
  }

  bool Delete(const GURL& origin, std::string key) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    stores_->DeleteStore(
        origin,
        key,
        base::Bind(&ServiceWorkerFetchStoresManagerTest::BoolAndErrorCallback,
                   base::Unretained(this),
                   base::Unretained(loop.get())));
    loop->Run();

    return callback_bool_;
  }

  bool Keys(const GURL& origin) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    stores_->EnumerateStores(
        origin,
        base::Bind(
            &ServiceWorkerFetchStoresManagerTest::StringsAndErrorCallback,
            base::Unretained(this),
            base::Unretained(loop.get())));
    loop->Run();

    bool error = callback_error_ !=
                 ServiceWorkerFetchStores::FETCH_STORES_ERROR_NO_ERROR;
    return !error;
  }

  bool VerifyKeys(const std::vector<std::string>& expected_keys) {
    if (expected_keys.size() != callback_strings_.size())
      return false;

    std::set<std::string> found_set;
    for (int i = 0, max = callback_strings_.size(); i < max; ++i)
      found_set.insert(callback_strings_[i]);

    for (int i = 0, max = expected_keys.size(); i < max; ++i) {
      if (found_set.find(expected_keys[i]) == found_set.end())
        return false;
    }
    return true;
  }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;

  base::ScopedTempDir temp_dir_;
  scoped_ptr<ServiceWorkerFetchStoresManager> stores_;

  int callback_bool_;
  int callback_store_;
  ServiceWorkerFetchStores::FetchStoresError callback_error_;
  std::vector<std::string> callback_strings_;

  const GURL origin1_;
  const GURL origin2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFetchStoresManagerTest);
};

class ServiceWorkerFetchStoresManagerMemoryOnlyTest
    : public ServiceWorkerFetchStoresManagerTest {
  virtual bool MemoryOnly() OVERRIDE { return true; }
};

class ServiceWorkerFetchStoresManagerTestP
    : public ServiceWorkerFetchStoresManagerTest,
      public testing::WithParamInterface<bool> {
  virtual bool MemoryOnly() OVERRIDE { return !GetParam(); }
};

TEST_F(ServiceWorkerFetchStoresManagerTest, TestsRunOnIOThread) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

TEST_P(ServiceWorkerFetchStoresManagerTestP, CreateStore) {
  EXPECT_TRUE(CreateStore(origin1_, "foo"));
}

TEST_P(ServiceWorkerFetchStoresManagerTestP, CreateDuplicateStore) {
  EXPECT_TRUE(CreateStore(origin1_, "foo"));
  EXPECT_FALSE(CreateStore(origin1_, "foo"));
  EXPECT_EQ(ServiceWorkerFetchStores::FETCH_STORES_ERROR_EXISTS,
            callback_error_);
}

TEST_P(ServiceWorkerFetchStoresManagerTestP, Create2Stores) {
  EXPECT_TRUE(CreateStore(origin1_, "foo"));
  EXPECT_TRUE(CreateStore(origin1_, "bar"));
}

TEST_P(ServiceWorkerFetchStoresManagerTestP, Create2StoresSameNameDiffSWs) {
  EXPECT_TRUE(CreateStore(origin1_, "foo"));
  EXPECT_TRUE(CreateStore(origin2_, "foo"));
}

TEST_P(ServiceWorkerFetchStoresManagerTestP, GetStore) {
  EXPECT_TRUE(CreateStore(origin1_, "foo"));
  int store = callback_store_;
  EXPECT_TRUE(Get(origin1_, "foo"));
  EXPECT_EQ(store, callback_store_);
}

TEST_P(ServiceWorkerFetchStoresManagerTestP, GetNonExistent) {
  EXPECT_FALSE(Get(origin1_, "foo"));
  EXPECT_EQ(ServiceWorkerFetchStores::FETCH_STORES_ERROR_NOT_FOUND,
            callback_error_);
}

TEST_P(ServiceWorkerFetchStoresManagerTestP, HasStore) {
  EXPECT_TRUE(CreateStore(origin1_, "foo"));
  EXPECT_TRUE(Has(origin1_, "foo"));
  EXPECT_TRUE(callback_bool_);
}

TEST_P(ServiceWorkerFetchStoresManagerTestP, HasNonExistent) {
  EXPECT_FALSE(Has(origin1_, "foo"));
}

TEST_P(ServiceWorkerFetchStoresManagerTestP, DeleteStore) {
  EXPECT_TRUE(CreateStore(origin1_, "foo"));
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_FALSE(Get(origin1_, "foo"));
  EXPECT_EQ(ServiceWorkerFetchStores::FETCH_STORES_ERROR_NOT_FOUND,
            callback_error_);
}

TEST_P(ServiceWorkerFetchStoresManagerTestP, DeleteTwice) {
  EXPECT_TRUE(CreateStore(origin1_, "foo"));
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_FALSE(Delete(origin1_, "foo"));
  EXPECT_EQ(ServiceWorkerFetchStores::FETCH_STORES_ERROR_NOT_FOUND,
            callback_error_);
}

TEST_P(ServiceWorkerFetchStoresManagerTestP, EmptyKeys) {
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_TRUE(callback_strings_.empty());
}

TEST_P(ServiceWorkerFetchStoresManagerTestP, SomeKeys) {
  EXPECT_TRUE(CreateStore(origin1_, "foo"));
  EXPECT_TRUE(CreateStore(origin1_, "bar"));
  EXPECT_TRUE(CreateStore(origin2_, "baz"));
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(2u, callback_strings_.size());
  std::vector<std::string> expected_keys;
  expected_keys.push_back("foo");
  expected_keys.push_back("bar");
  EXPECT_TRUE(VerifyKeys(expected_keys));
  EXPECT_TRUE(Keys(origin2_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ("baz", callback_strings_[0].c_str());
}

TEST_P(ServiceWorkerFetchStoresManagerTestP, DeletedKeysGone) {
  EXPECT_TRUE(CreateStore(origin1_, "foo"));
  EXPECT_TRUE(CreateStore(origin1_, "bar"));
  EXPECT_TRUE(CreateStore(origin2_, "baz"));
  EXPECT_TRUE(Delete(origin1_, "bar"));
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ("foo", callback_strings_[0].c_str());
}

TEST_P(ServiceWorkerFetchStoresManagerTestP, Chinese) {
  EXPECT_TRUE(CreateStore(origin1_, "你好"));
  int store = callback_store_;
  EXPECT_TRUE(Get(origin1_, "你好"));
  EXPECT_EQ(store, callback_store_);
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_TRUE("你好" == callback_strings_[0]);
}

TEST_F(ServiceWorkerFetchStoresManagerTest, EmptyKey) {
  EXPECT_FALSE(CreateStore(origin1_, ""));
  EXPECT_EQ(ServiceWorkerFetchStores::FETCH_STORES_ERROR_EMPTY_KEY,
            callback_error_);

  EXPECT_FALSE(Get(origin1_, ""));
  EXPECT_EQ(ServiceWorkerFetchStores::FETCH_STORES_ERROR_EMPTY_KEY,
            callback_error_);

  EXPECT_FALSE(Has(origin1_, ""));
  EXPECT_EQ(ServiceWorkerFetchStores::FETCH_STORES_ERROR_EMPTY_KEY,
            callback_error_);

  EXPECT_FALSE(Delete(origin1_, ""));
  EXPECT_EQ(ServiceWorkerFetchStores::FETCH_STORES_ERROR_EMPTY_KEY,
            callback_error_);
}

TEST_F(ServiceWorkerFetchStoresManagerTest, DataPersists) {
  EXPECT_TRUE(CreateStore(origin1_, "foo"));
  EXPECT_TRUE(CreateStore(origin1_, "bar"));
  EXPECT_TRUE(CreateStore(origin1_, "baz"));
  EXPECT_TRUE(CreateStore(origin2_, "raz"));
  EXPECT_TRUE(Delete(origin1_, "bar"));
  stores_ = ServiceWorkerFetchStoresManager::Create(stores_.get());
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(2u, callback_strings_.size());
  std::vector<std::string> expected_keys;
  expected_keys.push_back("foo");
  expected_keys.push_back("baz");
  EXPECT_TRUE(VerifyKeys(expected_keys));
}

TEST_F(ServiceWorkerFetchStoresManagerMemoryOnlyTest, DataLostWhenMemoryOnly) {
  EXPECT_TRUE(CreateStore(origin1_, "foo"));
  EXPECT_TRUE(CreateStore(origin2_, "baz"));
  stores_ = ServiceWorkerFetchStoresManager::Create(stores_.get());
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(0u, callback_strings_.size());
}

TEST_F(ServiceWorkerFetchStoresManagerTest, BadStoreName) {
  // Since the implementation writes store names to disk, ensure that we don't
  // escape the directory.
  const std::string bad_name = "../../../../../../../../../../../../../../foo";
  EXPECT_TRUE(CreateStore(origin1_, bad_name));
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ(bad_name.c_str(), callback_strings_[0].c_str());
}

TEST_F(ServiceWorkerFetchStoresManagerTest, BadOriginName) {
  // Since the implementation writes origin names to disk, ensure that we don't
  // escape the directory.
  GURL bad_origin("../../../../../../../../../../../../../../foo");
  EXPECT_TRUE(CreateStore(bad_origin, "foo"));
  EXPECT_TRUE(Keys(bad_origin));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ("foo", callback_strings_[0].c_str());
}

INSTANTIATE_TEST_CASE_P(ServiceWorkerFetchStoresManagerTests,
                        ServiceWorkerFetchStoresManagerTestP,
                        ::testing::Values(false, true));

}  // namespace content
