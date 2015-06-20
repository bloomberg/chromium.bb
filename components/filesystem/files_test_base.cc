// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/files_test_base.h"

#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "components/filesystem/public/interfaces/types.mojom.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/util/capture_util.h"

namespace filesystem {

FilesTestBase::FilesTestBase() {
}

FilesTestBase::~FilesTestBase() {
}

void FilesTestBase::SetUp() {
  ApplicationTestBase::SetUp();

  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:filesystem");
  application_impl()->ConnectToService(request.Pass(), &files_);
}

void FilesTestBase::GetTemporaryRoot(DirectoryPtr* directory) {
  FileError error = FILE_ERROR_FAILED;
  files()->OpenFileSystem("temp", GetProxy(directory), mojo::Capture(&error));
  ASSERT_TRUE(files().WaitForIncomingResponse());
  ASSERT_EQ(FILE_ERROR_OK, error);
}

}  // namespace filesystem
