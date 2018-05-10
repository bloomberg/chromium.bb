// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_bar_platform_helper.h"

#include "build/build_config.h"

#if !defined(OS_MACOSX)
// static
std::unique_ptr<FindBarPlatformHelper> FindBarPlatformHelper::Create(
    FindBarController* find_bar_controller) {
  return nullptr;
}
#endif

FindBarPlatformHelper::FindBarPlatformHelper(
    FindBarController* find_bar_controller)
    : find_bar_controller_(find_bar_controller) {}

FindBarPlatformHelper::~FindBarPlatformHelper() = default;
