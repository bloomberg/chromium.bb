// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_FILES_TEST_BASE_H_
#define COMPONENTS_FILESYSTEM_FILES_TEST_BASE_H_

#include "base/macros.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "mojo/application/public/cpp/application_test_base.h"

namespace filesystem {

class FilesTestBase : public mojo::test::ApplicationTestBase {
 public:
  FilesTestBase();
  ~FilesTestBase() override;

  void SetUp() override;

 protected:
  // Note: This has an out parameter rather than returning the |DirectoryPtr|,
  // since |ASSERT_...()| doesn't work with return values.
  void GetTemporaryRoot(DirectoryPtr* directory);

  FileSystemPtr& files() { return files_; }

 private:
  FileSystemPtr files_;

  DISALLOW_COPY_AND_ASSIGN(FilesTestBase);
};

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_FILES_TEST_BASE_H_
