// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Main function for common unittests.
#include <atlbase.h>
#include <atlcom.h>
#include "base/at_exit.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

// We're testing ATL code that requires a module object.
class ObligatoryModule: public CAtlDllModuleT<ObligatoryModule> {
};

ObligatoryModule g_obligatory_atl_module;

int main(int argc, char **argv) {
  base::AtExitManager exit_manager;
  testing::InitGoogleMock(&argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
