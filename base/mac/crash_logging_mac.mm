// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/crash_logging.h"

#import <Foundation/Foundation.h>

#include "base/debug/crash_logging.h"
#include "base/sys_string_conversions.h"

namespace base {
namespace mac {

void SetCrashKeyValue(NSString* key, NSString* val) {
  base::debug::SetCrashKeyValue(
      base::SysNSStringToUTF8(key),
      base::SysNSStringToUTF8(val));
}

void ClearCrashKey(NSString* key) {
  base::debug::ClearCrashKey(base::SysNSStringToUTF8(key));
}

void SetCrashKeyFromAddresses(NSString* key,
                              const void* const* addresses,
                              size_t count) {
  base::debug::SetCrashKeyFromAddresses(
      base::SysNSStringToUTF8(key),
      addresses,
      count);
}

ScopedCrashKey::ScopedCrashKey(NSString* key, NSString* value)
    : crash_key_([key retain]) {
  SetCrashKeyValue(key, value);
}

ScopedCrashKey::~ScopedCrashKey() {
  ClearCrashKey(crash_key_);
}

}  // namespace mac
}  // namespace base
