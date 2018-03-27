// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/passwords_directory_util.h"

#include "base/mac/bind_objc_block.h"
#include "base/task_scheduler/post_task.h"
#include "base/test/scoped_task_environment.h"
#import "ios/testing/wait_util.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::WaitUntilConditionOrTimeout;
using testing::kWaitForFileOperationTimeout;

using PasswordsDirectoryUtilTest = PlatformTest;

// Tests that DeletePasswordsDirectory() actually deletes the directory.
TEST_F(PasswordsDirectoryUtilTest, Deletion) {
  base::test::ScopedTaskEnvironment environment;

  NSURL* directory_url = GetPasswordsDirectoryURL();
  NSURL* file_url =
      [directory_url URLByAppendingPathComponent:@"TestPasswords.csv"];

  // Create a new file in the passwords  directory.
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindBlockArc(^() {
        NSFileManager* file_manager = [NSFileManager defaultManager];
        if ([file_manager createDirectoryAtURL:directory_url
                   withIntermediateDirectories:NO
                                    attributes:nil
                                         error:nil]) {
          [@"test" writeToURL:file_url
                   atomically:YES
                     encoding:NSUTF8StringEncoding
                        error:nil];
        }
      }));

  // Verify that the file was created in the passwords directory.
  EXPECT_TRUE(
      WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^bool() {
        return [[NSFileManager defaultManager] fileExistsAtPath:file_url.path];
      }));

  // Delete download directory.
  DeletePasswordsDirectory();

  // Verify passwords directory deletion.
  EXPECT_TRUE(
      WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^bool() {
        return ![[NSFileManager defaultManager] fileExistsAtPath:file_url.path];
      }));
}
