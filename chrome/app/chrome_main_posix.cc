// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main.h"

namespace chrome_main {

#if !defined(OS_MACOSX)
// This policy is not implemented for Linux and ChromeOS. The win and mac ver-
// sions are implemented in the platform specific version of chrome_main.cc e.g.
// chrome_main_win.cc or chrome_main_mac.mm .
void CheckUserDataDirPolicy(FilePath* user_data_dir) {}
#endif

}  // namespace chrome_main
