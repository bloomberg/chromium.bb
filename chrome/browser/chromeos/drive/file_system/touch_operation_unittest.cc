// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/touch_operation.h"

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace file_system {

typedef OperationTestBase TouchOperationTest;

TEST_F(TouchOperationTest, TouchFile) {
  TouchOperation operation(blocking_task_runner(),
                           observer(),
                           scheduler(),
                           metadata());

  const base::FilePath kTestPath(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  const base::Time::Exploded kLastAccessTime = {
    2012, 7, 0, 19, 15, 59, 13, 123
  };
  const base::Time::Exploded kLastModifiedTime = {
    2013, 7, 0, 19, 15, 59, 13, 123
  };

  FileError error = FILE_ERROR_FAILED;
  operation.TouchFile(
      kTestPath,
      base::Time::FromUTCExploded(kLastAccessTime),
      base::Time::FromUTCExploded(kLastModifiedTime),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(kTestPath, &entry));
  EXPECT_EQ(base::Time::FromUTCExploded(kLastAccessTime),
            base::Time::FromInternalValue(entry.file_info().last_accessed()));
  EXPECT_EQ(base::Time::FromUTCExploded(kLastModifiedTime),
            base::Time::FromInternalValue(entry.file_info().last_modified()));
}

}  // namespace file_system
}  // namespace drive
