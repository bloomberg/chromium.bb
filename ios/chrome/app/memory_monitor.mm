// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/memory_monitor.h"

#include <dispatch/dispatch.h>
#import <Foundation/NSPathUtilities.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/sys_info.h"
#import "ios/chrome/browser/crash_report/breakpad_helper.h"
#include "ios/web/public/web_thread.h"

namespace ios_internal {

void AsynchronousFreeMemoryMonitor() {
  UpdateBreakpadMemoryValues();
  web::WebThread::PostDelayedTask(
      web::WebThread::FILE, FROM_HERE,
      base::Bind(&ios_internal::AsynchronousFreeMemoryMonitor),
      base::TimeDelta::FromSeconds(30));
}

void UpdateBreakpadMemoryValues() {
  int freeMemory =
      static_cast<int>(base::SysInfo::AmountOfAvailablePhysicalMemory() / 1024);
  breakpad_helper::SetCurrentFreeMemoryInKB(freeMemory);
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                       NSUserDomainMask, YES);
  NSString* value = base::mac::ObjCCastStrict<NSString>([paths lastObject]);
  base::FilePath filePath = base::FilePath(base::SysNSStringToUTF8(value));
  int freeDiskSpace =
      static_cast<int>(base::SysInfo::AmountOfFreeDiskSpace(filePath) / 1024);
  breakpad_helper::SetCurrentFreeDiskInKB(freeDiskSpace);
}
}
