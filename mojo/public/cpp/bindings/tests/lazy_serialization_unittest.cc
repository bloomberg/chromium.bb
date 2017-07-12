// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/interfaces/bindings/tests/struct_with_traits.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class LazySerializationTest : public testing::Test {
 public:
  LazySerializationTest() {}
  ~LazySerializationTest() override {}

 private:
  base::test::ScopedTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(LazySerializationTest);
};

class TestUnserializedStructImpl : public test::TestUnserializedStruct {
 public:
  explicit TestUnserializedStructImpl(
      test::TestUnserializedStructRequest request)
      : binding_(this, std::move(request)) {}
  ~TestUnserializedStructImpl() override {}

  // test::TestUnserializedStruct:
  void PassUnserializedStruct(
      const test::StructWithUnreachableTraitsImpl& s,
      const PassUnserializedStructCallback& callback) override {
    callback.Run(s);
  }

 private:
  mojo::Binding<test::TestUnserializedStruct> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestUnserializedStructImpl);
};

TEST_F(LazySerializationTest, NeverSerialize) {
  // Basic sanity check to ensure that no messages are serialized by default in
  // environments where lazy serialization is supported, on an interface which
  // supports lazy serialization, and where both ends of the interface are in
  // the same process.

  test::TestUnserializedStructPtr ptr;
  TestUnserializedStructImpl impl(MakeRequest(&ptr));

  const int32_t kTestMagicNumber = 42;

  test::StructWithUnreachableTraitsImpl data;
  EXPECT_EQ(0, data.magic_number);
  data.magic_number = kTestMagicNumber;

  // Send our data over the pipe and wait for it to come back. The value should
  // be preserved. We know the data was never serialized because the
  // StructTraits for this type will DCHECK if executed in any capacity.
  int received_number = 0;
  base::RunLoop loop;
  ptr->PassUnserializedStruct(
      data, base::Bind(
                [](base::RunLoop* loop, int* received_number,
                   const test::StructWithUnreachableTraitsImpl& passed) {
                  *received_number = passed.magic_number;
                  loop->Quit();
                },
                &loop, &received_number));
  loop.Run();
  EXPECT_EQ(kTestMagicNumber, received_number);
}

}  // namespace
}  // namespace mojo
