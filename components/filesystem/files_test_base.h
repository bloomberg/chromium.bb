// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_FILES_TEST_BASE_H_
#define COMPONENTS_FILESYSTEM_FILES_TEST_BASE_H_

#include "base/macros.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "mojo/application/public/cpp/application_test_base.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace filesystem {

class FilesTestBase : public mojo::test::ApplicationTestBase,
                      public filesystem::FileSystemClient {
 public:
  FilesTestBase();
  ~FilesTestBase() override;

  // Overridden from mojo::test::ApplicationTestBase:
  void SetUp() override;

  // Overridden from FileSystemClient:
  void OnFileSystemShutdown() override;

 protected:
  // Note: This has an out parameter rather than returning the |DirectoryPtr|,
  // since |ASSERT_...()| doesn't work with return values.
  void GetTemporaryRoot(DirectoryPtr* directory);

  FileSystemPtr& files() { return files_; }

 private:
  mojo::Binding<filesystem::FileSystemClient> binding_;
  FileSystemPtr files_;

  DISALLOW_COPY_AND_ASSIGN(FilesTestBase);
};

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_FILES_TEST_BASE_H_
