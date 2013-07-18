// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breakpad/breakpad_client.h"

#include "base/logging.h"

namespace breakpad {

namespace {

BreakpadClient* g_client = NULL;

}  // namespace

void SetBreakpadClient(BreakpadClient* client) {
  g_client = client;
}

BreakpadClient* GetBreakpadClient() {
  DCHECK(g_client);
  return g_client;
}

BreakpadClient::BreakpadClient() {}
BreakpadClient::~BreakpadClient() {}

#if defined(OS_WIN)
bool BreakpadClient::GetAlternativeCrashDumpLocation(
    base::FilePath* crash_dir) {
  return false;
}
#endif

bool BreakpadClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
  return false;
}

#if defined(OS_POSIX)
void BreakpadClient::SetDumpWithoutCrashingFunction(void (*function)()) {
}
#endif

}  // namespace breakpad
