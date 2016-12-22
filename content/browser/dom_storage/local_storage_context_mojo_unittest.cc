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

}  // namespace

class LocalStorageContextMojoTest : public testing::Test {
 public:
  LocalStorageContextMojoTest()
      : db_(&mock_data_),
        db_binding_(&db_),
        context_(nullptr, base::FilePath()) {
    context_.SetDatabaseForTesting(db_binding_.CreateInterfacePtrAndBind());
  }

  LocalStorageContextMojo* context() { return &context_; }
  const std::map<std::vector<uint8_t>, std::vector<uint8_t>>& mock_data() {
    return mock_data_;
  }

 private:
  TestBrowserThreadBundle thread_bundle_;
  std::map<std::vector<uint8_t>, std::vector<uint8_t>> mock_data_;
  MockLevelDBDatabase db_;
  mojo::Binding<leveldb::mojom::LevelDBDatabase> db_binding_;

  LocalStorageContextMojo context_;
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
  ASSERT_EQ(1u, mock_data().size());
  EXPECT_EQ(value, mock_data().begin()->second);
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
  ASSERT_EQ(2u, mock_data().size());
  EXPECT_EQ(value, mock_data().begin()->second);
}

}  // namespace content
