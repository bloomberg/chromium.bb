// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/blacklist/crashpad_helper.h"

#include "third_party/crashpad/crashpad/client/crashpad_client.h"

int GenerateCrashDump(EXCEPTION_POINTERS* exception_pointers) {
  crashpad::CrashpadClient::DumpWithoutCrash(
      *(exception_pointers->ContextRecord));
  return EXCEPTION_CONTINUE_SEARCH;
}
