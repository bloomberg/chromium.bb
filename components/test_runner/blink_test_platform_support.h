// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_BLINK_TEST_PLATFORM_SUPPORT_H_
#define COMPONENTS_TEST_RUNNER_BLINK_TEST_PLATFORM_SUPPORT_H_

#include "components/test_runner/test_runner_export.h"

namespace test_runner {

bool TEST_RUNNER_EXPORT CheckLayoutSystemDeps();
bool TEST_RUNNER_EXPORT BlinkTestPlatformInitialize();

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_BLINK_TEST_PLATFORM_SUPPORT_H_
