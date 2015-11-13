// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/master_impl.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/test/test_utils.h"
#include "mojo/public/c/system/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace edk {
namespace {

static const char kHelloWorld[] = "hello world";

#if defined(OS_WIN)

class MasterImplTest : public ::testing::Test {
 public:
  MasterImplTest() {}

  // Returns a ScopedHandle to a file with the magic string.
  ScopedHandle GetScopedHandle() {
    if (!temp_dir_.IsValid())
      CHECK(temp_dir_.CreateUniqueTempDir());
    base::FilePath unused;
    base::ScopedFILE fp(
        CreateAndOpenTemporaryFileInDir(temp_dir_.path(), &unused));
    CHECK(fp);
    EXPECT_EQ(sizeof(kHelloWorld),
              fwrite(kHelloWorld, 1, sizeof(kHelloWorld), fp.get()));
    ScopedPlatformHandle platform_handle(
        test::PlatformHandleFromFILE(fp.Pass()));
    CHECK(platform_handle.is_valid());

    MojoHandle handle;
    MojoResult wrap_result = CreatePlatformHandleWrapper(
        platform_handle.Pass(), &handle);
    CHECK(wrap_result == MOJO_RESULT_OK);
    return ScopedHandle(Handle(handle));
  }

  // Check that the given ScopedHandle has a file with the magic string.
  bool CheckScopedHandle(ScopedHandle handle) {
    ScopedPlatformHandle platform_handle;
    MojoResult unwrap_result = PassWrappedPlatformHandle(
        handle.get().value(), &platform_handle);
    if (unwrap_result != MOJO_RESULT_OK)
      return false;
    base::ScopedFILE fp =
        test::FILEFromPlatformHandle(platform_handle.Pass(), "rb").Pass();
    if (!fp)
      return false;
    rewind(fp.get());
    char read_buffer[1000] = {};
    if (fread(read_buffer, 1, sizeof(read_buffer), fp.get()) !=
        sizeof(kHelloWorld)) {
      return false;
    }
    return std::string(read_buffer) == kHelloWorld;
  }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(MasterImplTest, Basic) {
  MasterImpl master(base::GetCurrentProcId());
  uint64_t token;
  master.HandleToToken(GetScopedHandle(),
                       [&token](int32_t result, uint64_t t) {
                         ASSERT_EQ(result, MOJO_RESULT_OK);
                         token = t;
                       });

  ScopedHandle handle;
  master.TokenToHandle(token,
                       [&handle](int32_t result, ScopedHandle h) {
                         ASSERT_EQ(result, MOJO_RESULT_OK);
                         handle = h.Pass();
                       });

  ASSERT_TRUE(CheckScopedHandle(handle.Pass()));
}

TEST_F(MasterImplTest, TokenIsRemoved) {
  MasterImpl master(base::GetCurrentProcId());
  uint64_t token;
  master.HandleToToken(GetScopedHandle(),
                       [&token](int32_t result, uint64_t t) {
                         ASSERT_EQ(result, MOJO_RESULT_OK);
                         token = t;
                       });

  ScopedHandle handle;
  master.TokenToHandle(token,
                       [&handle](int32_t result, ScopedHandle h) {
                         ASSERT_EQ(result, MOJO_RESULT_OK);
                         handle = h.Pass();
                       });

  ASSERT_TRUE(CheckScopedHandle(handle.Pass()));

  master.TokenToHandle(token,
                       [&handle](int32_t result, ScopedHandle h) {
                         ASSERT_EQ(result, MOJO_RESULT_INVALID_ARGUMENT);
                       });
}

#endif  // OS_WIN

}  // namespace
}  // namespace edk
}  // namespace mojo
