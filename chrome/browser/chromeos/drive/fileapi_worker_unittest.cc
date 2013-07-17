// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/fileapi_worker.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/dummy_file_system.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace fileapi_internal {
namespace {

// Increments |num_called| for checking how many times the closure is called.
void Increment(int* num_called) {
  ++*num_called;
}

// Returns the |instance| as is.
FileSystemInterface* GetFileSystem(FileSystemInterface* instance) {
  return instance;
}

}  // namespace

class FileApiWorkerTest : public testing::Test {
 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(FileApiWorkerTest, RunFileSystemCallbackSuccess) {
  DummyFileSystem dummy_file_system;

  FileSystemInterface* file_system = NULL;
  RunFileSystemCallback(
      base::Bind(&GetFileSystem, &dummy_file_system),
      google_apis::test_util::CreateCopyResultCallback(&file_system),
      base::Closure());

  EXPECT_EQ(&dummy_file_system, file_system);
}

TEST_F(FileApiWorkerTest, RunFileSystemCallbackFail) {
  FileSystemInterface* file_system = NULL;

  // Make sure on_error_callback is called if file_system_getter returns NULL.
  int num_called = 0;
  RunFileSystemCallback(
      base::Bind(&GetFileSystem, static_cast<FileSystemInterface*>(NULL)),
      google_apis::test_util::CreateCopyResultCallback(&file_system),
      base::Bind(&Increment, &num_called));
  EXPECT_EQ(1, num_called);

  // Just make sure this null |on_error_callback| doesn't cause a crash.
  RunFileSystemCallback(
      base::Bind(&GetFileSystem, static_cast<FileSystemInterface*>(NULL)),
      google_apis::test_util::CreateCopyResultCallback(&file_system),
      base::Closure());
}

}  // namespace fileapi_internal
}  // namespace drive
