// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace headless {

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, CreateAndDestroyWebContents) {
  scoped_ptr<HeadlessWebContents> web_contents =
      browser()->CreateWebContents(gfx::Size(800, 600));
  EXPECT_TRUE(web_contents);
  // TODO(skyostil): Verify viewport dimensions once we can.
  web_contents.reset();
}

}  // namespace headless
