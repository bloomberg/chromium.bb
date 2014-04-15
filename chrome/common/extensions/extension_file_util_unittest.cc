// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_file_util.h"

#include <set>

#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

typedef testing::Test ExtensionFileUtilTest;

// Test that a browser action extension returns a path to an icon.
TEST_F(ExtensionFileUtilTest, GetBrowserImagePaths) {
  base::FilePath install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extensions")
                    .AppendASCII("api_test")
                    .AppendASCII("browser_action")
                    .AppendASCII("basics");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      install_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get());

  // The extension contains one icon.
  std::set<base::FilePath> paths =
      extension_file_util::GetBrowserImagePaths(extension.get());
  ASSERT_EQ(1u, paths.size());
  EXPECT_EQ("icon.png", paths.begin()->BaseName().AsUTF8Unsafe());
}

}  // namespace extensions
