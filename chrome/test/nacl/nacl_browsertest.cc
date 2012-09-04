// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/nacl/nacl_browsertest_util.h"

namespace {

// Disable tests under Linux ASAN.
// Linux ASAN doesn't work with NaCl.  See: http://crbug.com/104832.
// TODO(ncbray) enable after http://codereview.chromium.org/10830009/ lands.
#if !(defined(ADDRESS_SANITIZER) && defined(OS_LINUX))

NACL_BROWSER_TEST_F(NaClBrowserTest, SimpleLoad, {
  RunLoadTest(FILE_PATH_LITERAL("nacl_load_test.html"));
})

#endif  // !(defined(ADDRESS_SANITIZER) && defined(OS_LINUX))

}  // namespace anonymous
