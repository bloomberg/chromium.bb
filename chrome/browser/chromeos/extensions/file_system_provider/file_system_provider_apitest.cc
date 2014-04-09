// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

namespace extensions {

class FileSystemProviderApiTest : public ExtensionApiTest {
 public:
  FileSystemProviderApiTest()
      // Set the channel to "trunk" since this API is restricted to trunk.
      : current_channel_(chrome::VersionInfo::CHANNEL_UNKNOWN) {
  }

 private:
  extensions::ScopedCurrentChannel current_channel_;
};

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Mount) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/mount",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Unmount) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/unmount",
                                          kFlagLoadAsComponent))
      << message_;
}

}  // namespace extensions
