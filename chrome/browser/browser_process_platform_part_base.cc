// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_base.h"

BrowserProcessPlatformPartBase::BrowserProcessPlatformPartBase() {
}

BrowserProcessPlatformPartBase::~BrowserProcessPlatformPartBase() {
}

void BrowserProcessPlatformPartBase::PlatformSpecificCommandLineProcessing(
    const CommandLine& /* command_line */) {
}

void BrowserProcessPlatformPartBase::StartTearDown() {
}
