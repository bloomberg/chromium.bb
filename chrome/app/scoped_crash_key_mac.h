// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_SCOPED_CRASH_KEY_MAC_H_
#define CHROME_APP_SCOPED_CRASH_KEY_MAC_H_
#pragma once

#import <Foundation/Foundation.h>

#import "base/memory/scoped_nsobject.h"
#import "chrome/app/breakpad_mac.h"

// This helper can be used to add additional breakpad keys when some
// code is known to crash.  It should only be used when more
// traditional debugging has not been able to reproduce the problem.

class ScopedCrashKey {
 public:
  ScopedCrashKey(NSString* key, NSString* value)
      : crash_key_([key retain]) {
    SetCrashKeyValue(crash_key_.get(), value);
  }
  ~ScopedCrashKey() {
    ClearCrashKeyValue(crash_key_.get());
  }

 private:
  scoped_nsobject<NSString> crash_key_;
};

#endif  // CHROME_APP_SCOPED_CRASH_KEY_MAC_H_
