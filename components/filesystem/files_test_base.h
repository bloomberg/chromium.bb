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
