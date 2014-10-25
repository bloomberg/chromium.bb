// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/chrome/athena_browsertest.h"

#include "athena/test/chrome/test_util.h"
#include "base/command_line.h"

namespace athena {

namespace {
const char kNoNaclSandbox[] = "--no-sandbox";
}

AthenaBrowserTest::AthenaBrowserTest() {
}

AthenaBrowserTest::~AthenaBrowserTest() {
}

void AthenaBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  // The NaCl sandbox won't work in our browser tests.
  command_line->AppendSwitch(kNoNaclSandbox);
  InProcessBrowserTest::SetUpCommandLine(command_line);
}

void AthenaBrowserTest::SetUpOnMainThread() {
  // Set the memory pressure to low and turning off undeterministic resource
  // observer events.
  test_util::SendTestMemoryPressureEvent(ResourceManager::MEMORY_PRESSURE_LOW);
}

}  // namespace athena
