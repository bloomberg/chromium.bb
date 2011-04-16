// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#if defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, LocalFileSystem) {
  ASSERT_TRUE(RunComponentExtensionTest("local_filesystem")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FileBrowserTest) {
  ASSERT_TRUE(RunExtensionTest("filesystem_handler")) << message_;
  ASSERT_TRUE(RunComponentExtensionTest("filebrowser_component")) << message_;
}

#endif
