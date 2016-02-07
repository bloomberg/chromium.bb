// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/files_test_base.h"

#include <utility>

#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "components/filesystem/public/interfaces/types.mojom.h"
#include "mojo/shell/public/cpp/shell.h"
#include "mojo/util/capture_util.h"

namespace filesystem {

FilesTestBase::FilesTestBase() : binding_(this) {
}

FilesTestBase::~FilesTestBase() {
}

void FilesTestBase::SetUp() {
  ApplicationTestBase::SetUp();
  shell()->ConnectToService("mojo:filesystem", &files_);
}

void FilesTestBase::OnFileSystemShutdown() {
}

void FilesTestBase::GetTemporaryRoot(DirectoryPtr* directory) {
  FileError error = FileError::FAILED;
  files()->OpenFileSystem("temp", GetProxy(directory),
                          binding_.CreateInterfacePtrAndBind(),
                          mojo::Capture(&error));
  ASSERT_TRUE(files().WaitForIncomingResponse());
  ASSERT_EQ(FileError::OK, error);
}

}  // namespace filesystem
