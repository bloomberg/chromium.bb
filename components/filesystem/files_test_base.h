// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_FILES_TEST_BASE_H_
#define COMPONENTS_FILESYSTEM_FILES_TEST_BASE_H_

#include "base/macros.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/shell/public/cpp/shell_test.h"

namespace filesystem {

template <typename T1>
mojo::Callback<void(T1)> Capture(T1* t1) {
  return [t1](T1 got_t1) { *t1 = std::move(got_t1); };
}

template <typename T1, typename T2>
mojo::Callback<void(T1, T2)> Capture(T1* t1, T2* t2) {
  return [t1, t2](T1 got_t1, T2 got_t2) {
    *t1 = std::move(got_t1);
    *t2 = std::move(got_t2);
  };
}

template <typename T1, typename T2, typename T3>
mojo::Callback<void(T1, T2, T3)> Capture(T1* t1, T2* t2, T3* t3) {
  return [t1, t2, t3](T1 got_t1, T2 got_t2, T3 got_t3) {
    *t1 = std::move(got_t1);
    *t2 = std::move(got_t2);
    *t3 = std::move(got_t3);
  };
}

class FilesTestBase : public shell::test::ShellTest {
 public:
  FilesTestBase();
  ~FilesTestBase() override;

  // Overridden from shell::test::ShellTest:
  void SetUp() override;

 protected:
  // Note: This has an out parameter rather than returning the |DirectoryPtr|,
  // since |ASSERT_...()| doesn't work with return values.
  void GetTemporaryRoot(mojom::DirectoryPtr* directory);

  mojom::FileSystemPtr& files() { return files_; }

 private:
  mojom::FileSystemPtr files_;

  DISALLOW_COPY_AND_ASSIGN(FilesTestBase);
};

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_FILES_TEST_BASE_H_
