// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DNRPageWhitelisting) {
  base::FilePath extension_path =
      test_data_dir_.AppendASCII("declarative_net_request");

  // Copy the extension directory to a temporary location. We do this to ensure
  // that the temporary kMetadata folder created as a result of loading the
  // extension is not written to the src directory and is automatically removed.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::CopyDirectory(extension_path, temp_dir.GetPath(), true /*recursive*/);

  // Override the path used for loading the extension.
  test_data_dir_ = temp_dir.GetPath();
  ASSERT_TRUE(RunExtensionTest("declarative_net_request")) << message_;
}
