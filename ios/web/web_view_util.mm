// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_view_util.h"

#include <Foundation/Foundation.h>
#include <sys/sysctl.h>

#include "base/ios/ios_util.h"

namespace web {

bool IsWKWebViewSupported() {
  // WKWebView is available starting from iOS8.
  if (!base::ios::IsRunningOnIOS8OrLater())
    return false;

// WKWebView does not work with 32-bit binaries running on 64-bit hardware.
// (rdar://18268087)
#if !defined(__LP64__)
  NSString* simulator_model_id =
      [[NSProcessInfo processInfo] environment][@"SIMULATOR_MODEL_IDENTIFIER"];
  // For simulator it's not possible to query hw.cpu64bit_capable value as the
  // function will return information for mac hardware rather than simulator.
  if (simulator_model_id) {
    // A set of known 32-bit simulators that are also iOS 8 compatible.
    // (taken from iOS 8.1 SDK simulators list).
    NSSet* known_32_bit_devices = [NSSet
        setWithArray:@[ @"iPad2,1", @"iPad3,1", @"iPhone4,1", @"iPhone5,1" ]];
    return [known_32_bit_devices containsObject:simulator_model_id];
  }
  uint32_t cpu64bit_capable = 0;
  size_t sizes = sizeof(cpu64bit_capable);
  sysctlbyname("hw.cpu64bit_capable", &cpu64bit_capable, &sizes, NULL, 0);
  return !cpu64bit_capable;
#else
  return true;
#endif
}

}  // namespace web
