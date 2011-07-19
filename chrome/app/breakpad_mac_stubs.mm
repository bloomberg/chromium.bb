// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/app/breakpad_mac.h"

#import <Foundation/Foundation.h>

// Stubbed out versions of breakpad integration functions so we can compile
// without linking in Breakpad.

bool IsCrashReporterEnabled() {
  return false;
}

void InitCrashProcessInfo() {
}

void DestructCrashReporter() {
}

void InitCrashReporter() {
}

void SetCrashKeyValue(NSString* key, NSString* value) {
}

void ClearCrashKeyValue(NSString* key) {
}

// NOTE(shess): These functions could clearly be replaced by stubs,
// but since they are seldom-used helpers, it seemed more reasonable
// to duplicate them from the primary implementation in
// breakpad_mac.mm.
ScopedCrashKey::ScopedCrashKey(NSString* key, NSString* value)
    : crash_key_([key retain]) {
  SetCrashKeyValue(crash_key_.get(), value);
}

ScopedCrashKey::~ScopedCrashKey() {
  ClearCrashKeyValue(crash_key_.get());
}
