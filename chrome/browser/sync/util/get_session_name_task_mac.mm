// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/get_session_name_task.h"

#import <Foundation/Foundation.h>
#import <SystemConfiguration/SCDynamicStoreCopySpecific.h>
#include <sys/sysctl.h>  // sysctlbyname()

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/sys_string_conversions.h"

@interface NSHost(SnowLeopardAPI)
- (NSString*)localizedName;
@end

namespace browser_sync {

// static
std::string GetSessionNameTask::GetHardwareModelName() {
  NSHost* myHost = [NSHost currentHost];
  if ([myHost respondsToSelector:@selector(localizedName)])
    return base::SysNSStringToUTF8([myHost localizedName]);

  // Fallback for 10.5
  scoped_nsobject<NSString> computerName(base::mac::CFToNSCast(
      SCDynamicStoreCopyComputerName(NULL, NULL)));
  if (computerName.get() != NULL)
    return base::SysNSStringToUTF8(computerName.get());

  // If all else fails, return to using a slightly nicer version of the
  // hardware model.
  char modelBuffer[256];
  size_t length = sizeof(modelBuffer);
  if (!sysctlbyname("hw.model", modelBuffer, &length, NULL, 0)) {
    for (size_t i = 0; i < length; i++) {
      if (IsAsciiDigit(modelBuffer[i]))
        return std::string(modelBuffer, 0, i);
    }
    return std::string(modelBuffer, 0, length);
  }
  return "Unknown";
}

}  // namespace browser_sync
