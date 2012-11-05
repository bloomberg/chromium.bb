// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_COMMON_H_
#define CC_TEST_TEST_COMMON_H_

#include "cc/settings.h"

namespace WebKitTests {

// If you have a test that modifies or uses global settings, keep an instance
// of this class to ensure that you start and end with a clean slate.
class ScopedSettings {
public:
    ScopedSettings() { cc::Settings::resetForTest(); }
    ~ScopedSettings() { cc::Settings::resetForTest(); }
};

} // namespace WebKitTests

#endif  // CC_TEST_TEST_COMMON_H_
