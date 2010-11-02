// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unittests for the gflags utilities.
#include "gtest/gtest.h"
#include "ceee/testing/utils/gflag_utils.h"
#include "ceee/testing/utils/nt_internals.h"

namespace testing {

TEST(GFlagUtilsTest, WriteProcessGFlags) {
  // Get our own global flags.
  ULONG old_gflags = nt_internals::RtlGetNtGlobalFlags();

  // Toggle one bit.
  ULONG new_gflags = old_gflags ^ FLG_SHOW_LDR_SNAPS;
  EXPECT_HRESULT_SUCCEEDED(WriteProcessGFlags(::GetCurrentProcess(),
                                              new_gflags));
  EXPECT_EQ(new_gflags, nt_internals::RtlGetNtGlobalFlags());

  // Restore old global flags.
  EXPECT_HRESULT_SUCCEEDED(WriteProcessGFlags(::GetCurrentProcess(),
                                              old_gflags));
  EXPECT_EQ(old_gflags, nt_internals::RtlGetNtGlobalFlags());
}

}  // namespace testing
