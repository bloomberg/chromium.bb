// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

// An empty test (it starts up and shuts down the browser as part of its
// setup and teardown) used to prefetch all of the browser code into memory.
IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, Empty) {
}
