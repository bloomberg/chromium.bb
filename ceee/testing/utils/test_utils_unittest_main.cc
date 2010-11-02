// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Main function for common unittests.
#include <atlbase.h>
#include <atlcom.h>
#include "gtest/gtest.h"
#include "ceee/testing/utils/gflag_utils.h"

// The global flags we want these tests to run with.
const DWORD kGlobalFlags = FLG_USER_STACK_TRACE_DB;

// We're testing ATL code that requires a module object.
class ObligatoryModule: public CAtlDllModuleT<ObligatoryModule> {
};

ObligatoryModule g_obligatory_atl_module;

int main(int argc, char **argv) {
  if (!IsDebuggerPresent())
    testing::RelaunchWithGFlags(kGlobalFlags);

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
