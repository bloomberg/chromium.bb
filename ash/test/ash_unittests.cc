// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_suite.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

int main(int argc, char** argv) {
#if defined(OS_WIN)
  // Disabled on Win8 until they're passing cleanly. http://crbug.com/154081
  if (base::win::GetVersion() >= base::win::VERSION_WIN8)
    return 0;
#endif
  return ash::test::AuraShellTestSuite(argc, argv).Run();
}
