// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELF_BLACKLIST_CRASHPAD_HELPER_H_
#define CHROME_ELF_BLACKLIST_CRASHPAD_HELPER_H_

#include <windows.h>

// Exception handler for exceptions in chrome_elf which need to be passed on to
// the next handler in the chain. Examples include exceptions in DllMain,
// blacklist interception code, etc.
int GenerateCrashDump(EXCEPTION_POINTERS* exception_pointers);

#endif  // CHROME_ELF_BLACKLIST_CRASHPAD_HELPER_H_
