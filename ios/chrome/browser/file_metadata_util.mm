// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/file_metadata_util.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"

namespace file_metadata_util {

void AddSkipSystemBackupAttributeToItem(const base::FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();

  NSURL* file_url =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];
  DCHECK([[NSFileManager defaultManager] fileExistsAtPath:file_url.path]);

  NSError* error = nil;
  BOOL success = [file_url setResourceValue:@YES
                                     forKey:NSURLIsExcludedFromBackupKey
                                      error:&error];
  if (!success) {
    LOG(ERROR) << [[error description] UTF8String];
  }
}

}  // namespace file_metadata_util
