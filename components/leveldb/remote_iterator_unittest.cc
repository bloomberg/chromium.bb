// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "components/leveldb/public/cpp/remote_iterator.h"
#include "components/leveldb/public/interfaces/leveldb.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "services/shell/public/cpp/service_context.h"
#include "services/shell/public/cpp/service_test.h"

namespace leveldb {
namespace {

template <typename T>
void DoCapture(T* t, const base::Closure& quit_closure, T got_t) {
  *t = std::move(got_t);
  if (!quit_closure.is_null())
    quit_closure.Run();
}

template <typename T1>
base::Callback<void(T1)> Capture(
    T1* t1,
    const base::Closure& quit_closure = base::Closure()) {
  return base::Bind(&DoCapture<T1>, t1, quit_closure);
}

class RemoteIteratorTest : public shell::test::ServiceTest {
 public:
  RemoteIteratorTest() : ServiceTest("exe:leveldb_service_unittests") {}
  ~RemoteIteratorTest() override {}

 protected:
  // Overridden from mojo::test::ApplicationTestBase:
  void SetUp() override {
    ServiceTest::SetUp();
    connector()->ConnectToInterface("mojo:leveldb", &leveldb_);

    mojom::DatabaseError error;
    base::RunLoop run_loop;
    leveldb()->OpenInMemory(GetProxy(&database_),
                            Capture(&error, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(mojom::DatabaseError::OK, error);

    std::map<std::string, std::string> data{
        {"a", "first"}, {"b:suffix", "second"}, {"c", "third"}};

    for (auto p : data) {
      // Write a key to the database.
      error = mojom::DatabaseError::INVALID_ARGUMENT;
      base::RunLoop run_loop;
      database_->Put(mojo::Array<uint8_t>::From(p.first),
                     mojo::Array<uint8_t>::From(p.second),
                     Capture(&error, run_loop.QuitClosure()));
      run_loop.Run();
      EXPECT_EQ(mojom::DatabaseError::OK, error);
    }
  }

  void TearDown() override {
    leveldb_.reset();
    ServiceTest::TearDown();
  }

  mojom::LevelDBServicePtr& leveldb() { return leveldb_; }
  mojom::LevelDBDatabasePtr& database() { return database_; }

 private:
  mojom::LevelDBServicePtr leveldb_;
  mojom::LevelDBDatabasePtr database_;

  DISALLOW_COPY_AND_ASSIGN(RemoteIteratorTest);
};

TEST_F(RemoteIteratorTest, Seeking) {
  uint64_t iterator_id = 0;
  base::RunLoop run_loop;
  database()->NewIterator(Capture(&iterator_id, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_NE(0u, iterator_id);

  RemoteIterator it(database().get(), iterator_id);
  EXPECT_FALSE(it.Valid());

  it.SeekToFirst();
  EXPECT_TRUE(it.Valid());
  EXPECT_EQ("a", it.key());
  EXPECT_EQ("first", it.value());

  it.SeekToLast();
  EXPECT_TRUE(it.Valid());
  EXPECT_EQ("c", it.key());
  EXPECT_EQ("third", it.value());

  it.Seek("b");
  EXPECT_TRUE(it.Valid());
  EXPECT_EQ("b:suffix", it.key());
  EXPECT_EQ("second", it.value());
}

TEST_F(RemoteIteratorTest, Next) {
  uint64_t iterator_id = 0;
  base::RunLoop run_loop;
  database()->NewIterator(Capture(&iterator_id, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_NE(0u, iterator_id);

  RemoteIterator it(database().get(), iterator_id);
  EXPECT_FALSE(it.Valid());

  it.SeekToFirst();
  EXPECT_TRUE(it.Valid());
  EXPECT_EQ("a", it.key());
  EXPECT_EQ("first", it.value());

  it.Next();
  EXPECT_TRUE(it.Valid());
  EXPECT_EQ("b:suffix", it.key());
  EXPECT_EQ("second", it.value());

  it.Next();
  EXPECT_TRUE(it.Valid());
  EXPECT_EQ("c", it.key());
  EXPECT_EQ("third", it.value());

  it.Next();
  EXPECT_FALSE(it.Valid());
}

TEST_F(RemoteIteratorTest, Prev) {
  uint64_t iterator_id = 0;
  base::RunLoop run_loop;
  database()->NewIterator(Capture(&iterator_id, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_NE(0u, iterator_id);

  RemoteIterator it(database().get(), iterator_id);
  EXPECT_FALSE(it.Valid());

  it.SeekToLast();
  EXPECT_TRUE(it.Valid());
  EXPECT_EQ("c", it.key());
  EXPECT_EQ("third", it.value());

  it.Prev();
  EXPECT_TRUE(it.Valid());
  EXPECT_EQ("b:suffix", it.key());
  EXPECT_EQ("second", it.value());

  it.Prev();
  EXPECT_TRUE(it.Valid());
  EXPECT_EQ("a", it.key());
  EXPECT_EQ("first", it.value());

  it.Prev();
  EXPECT_FALSE(it.Valid());
}

}  // namespace
}  // namespace leveldb
