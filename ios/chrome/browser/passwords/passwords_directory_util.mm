// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/passwords_directory_util.h"

#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_restrictions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Synchronously deletes passwords directoy.
void DeletePasswordsDirectorySync() {
  NSFileManager* file_manager = [NSFileManager defaultManager];
  base::AssertBlockingAllowed();
  NSURL* passwords_directory = GetPasswordsDirectoryURL();
  [file_manager removeItemAtURL:passwords_directory error:nil];
}
}  // namespace

NSURL* GetPasswordsDirectoryURL() {
  return [[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES]
      URLByAppendingPathComponent:@"passwords"
                      isDirectory:YES];
}

void DeletePasswordsDirectory() {
  base::PostTaskWithTraits(FROM_HERE,
                           {base::MayBlock(), base::TaskPriority::BACKGROUND},
                           base::BindOnce(&DeletePasswordsDirectorySync));
}
