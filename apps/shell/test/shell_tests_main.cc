// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "apps/shell/test/shell_test_launcher_delegate.h"
#include "base/sys_info.h"
#include "testing/gtest/include/gtest/gtest.h"

int main(int argc, char** argv) {
  int default_jobs = std::max(1, base::SysInfo::NumberOfProcessors() / 2);
  apps::AppShellTestLauncherDelegate launcher_delegate;
  return content::LaunchTests(&launcher_delegate, default_jobs, argc, argv);
}
