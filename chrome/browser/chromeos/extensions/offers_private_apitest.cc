// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

class ExtensionOffersPrivateApiTest : public ExtensionApiTest {
};

IN_PROC_BROWSER_TEST_F(ExtensionOffersPrivateApiTest, OffersTest) {
  EXPECT_TRUE(RunComponentExtensionTest("offers/component_extension"))
      << message_;
}
