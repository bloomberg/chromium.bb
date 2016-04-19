// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/common/common_custom_types.mojom.h"
#include "mojo/common/test_common_custom_types.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace common {
namespace test {

class TestFilePathImpl : public TestFilePath {
 public:
  explicit TestFilePathImpl(TestFilePathRequest request)
      : binding_(this, std::move(request)) {}

  // TestFilePath implementation:
  void BounceFilePath(const base::FilePath& in,
                      const BounceFilePathCallback& callback) override {
    callback.Run(in);
  }

 private:
  mojo::Binding<TestFilePath> binding_;
};

TEST(CommonCustomTypesTest, FilePath) {
  base::MessageLoop message_loop;
  base::RunLoop run_loop;

  TestFilePathPtr ptr;
  TestFilePathImpl impl(GetProxy(&ptr));

  base::FilePath dir(FILE_PATH_LITERAL("hello"));
  base::FilePath file = dir.Append(FILE_PATH_LITERAL("world"));

  ptr->BounceFilePath(file, [&run_loop, &file](const base::FilePath& out) {
    EXPECT_EQ(file, out);
    run_loop.Quit();
  });

  run_loop.Run();
}

}  // namespace test
}  // namespace common
}  // namespace mojo
