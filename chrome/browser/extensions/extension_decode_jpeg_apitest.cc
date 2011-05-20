// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#if defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DecodeJPEG) {
  ASSERT_TRUE(RunExtensionTest("decode_jpeg")) << message_;
}

#endif
