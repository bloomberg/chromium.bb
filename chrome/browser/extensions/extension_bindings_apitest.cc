// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains holistic tests of the bindings infrastructure

#include "chrome/browser/extensions/extension_apitest.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ExceptionInHandlerShouldNotCrash) {
  ASSERT_TRUE(RunExtensionSubtest(
      "bindings/exception_in_handler_should_not_crash",
      "page.html")) << message_;
}
