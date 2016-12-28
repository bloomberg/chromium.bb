// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/local_storage_context_mojo.h"

#include "base/run_loop.h"
#include "components/leveldb/public/cpp/util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/mock_leveldb_database.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb::StdStringToUint8Vector;
using leveldb::Uint8VectorToStdString;

namespace content {

namespace {

void NoOpSuccess(bool success) {}

void GetCallback(const base::Closure& callback,
                 bool* success_out,
                 std::vector<uint8_t>* value_out,
                 bool success,
                 const std::vector<uint8_t>& value) {
  *success_out = success;
  *value_out = value;
  callback.Run();
}

}  // namespace

class LocalStorageContextMojoTest : public testing::Test {
 public:
  LocalStorageContextMojoTest() : db_(&mock_data_), db_binding_(&db_) {}

  LocalStorageContextMojo* context() {
    if (!context_) {
      context_ =
          base::MakeUnique<LocalStorageContextMojo>(nullptr, base::FilePath());
      context_->SetDatabaseForTesting(db_binding_.CreateInterfacePtrAndBind());
    }
    return context_.get();
  }
  const std::map<std::vector<uint8_t>, std::vector<uint8_t>>& mock_data() {
    return mock_data_;
  }

  void set_mock_data(const std::string& key, const std::string& value) {
    mock_data_[StdStringToUint8Vector(key)] = StdStringToUint8Vector(value);
  }

 private:
  TestBrowserThreadBundle thread_bundle_;
  std::map<std::vector<uint8_t>, std::vector<uint8_t>> mock_data_;
  MockLevelDBDatabase db_;
  mojo::Binding<leveldb::mojom::LevelDBDatabase> db_binding_;

  std::unique_ptr<LocalStorageContextMojo> context_;
};

TEST_F(LocalStorageContextMojoTest, Basic) {
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojom::LevelDBWrapperPtr wrapper;
  context()->OpenLocalStorage(url::Origin(GURL("http://foobar.com")),
                              MakeRequest(&wrapper));
  wrapper->Put(key, value, "source", base::Bind(&NoOpSuccess));
  wrapper.reset();

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, mock_data().size());
  EXPECT_EQ(value, mock_data().rbegin()->second);
}

TEST_F(LocalStorageContextMojoTest, OriginsAreIndependent) {
  url::Origin origin1(GURL("http://foobar.com:123"));
  url::Origin origin2(GURL("http://foobar.com:1234"));
  auto key1 = StdStringToUint8Vector("4key");
  auto key2 = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojom::LevelDBWrapperPtr wrapper;
  context()->OpenLocalStorage(origin1, MakeRequest(&wrapper));
  wrapper->Put(key1, value, "source", base::Bind(&NoOpSuccess));
  wrapper.reset();

  context()->OpenLocalStorage(origin2, MakeRequest(&wrapper));
  wrapper->Put(key2, value, "source", base::Bind(&NoOpSuccess));
  wrapper.reset();

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, mock_data().size());
  EXPECT_EQ(value, mock_data().rbegin()->second);
}

TEST_F(LocalStorageContextMojoTest, ValidVersion) {
  set_mock_data("VERSION", "1");
  set_mock_data(std::string("_http://foobar.com") + '\x00' + "key", "value");

  mojom::LevelDBWrapperPtr wrapper;
  context()->OpenLocalStorage(url::Origin(GURL("http://foobar.com")),
                              MakeRequest(&wrapper));

  base::RunLoop run_loop;
  bool success = false;
  std::vector<uint8_t> result;
  wrapper->Get(
      StdStringToUint8Vector("key"),
      base::Bind(&GetCallback, run_loop.QuitClosure(), &success, &result));
  run_loop.Run();
  EXPECT_TRUE(success);
  EXPECT_EQ(StdStringToUint8Vector("value"), result);
}

TEST_F(LocalStorageContextMojoTest, InvalidVersion) {
  set_mock_data("VERSION", "foobar");
  set_mock_data(std::string("_http://foobar.com") + '\x00' + "key", "value");

  mojom::LevelDBWrapperPtr wrapper;
  context()->OpenLocalStorage(url::Origin(GURL("http://foobar.com")),
                              MakeRequest(&wrapper));

  base::RunLoop run_loop;
  bool success = false;
  std::vector<uint8_t> result;
  wrapper->Get(
      StdStringToUint8Vector("key"),
      base::Bind(&GetCallback, run_loop.QuitClosure(), &success, &result));
  run_loop.Run();
  EXPECT_FALSE(success);
}

}  // namespace content
