// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/simple_resource_loader.h"

#include "base/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SimpleResourceLoaderTest, GetLocaleFilePath) {
  SimpleResourceLoader loader;

  FilePath file_path;
  // Test valid language-region string:
  EXPECT_TRUE(loader.GetLocaleFilePath(L"en", L"GB", &file_path));
  EXPECT_TRUE(file_path.BaseName() == FilePath(L"en-GB.dll"));

  // Test valid language-region string for which we only have a language dll:
  EXPECT_TRUE(loader.GetLocaleFilePath(L"fr", L"FR", &file_path));
  EXPECT_TRUE(file_path.BaseName() == FilePath(L"fr.dll"));

  // Test invalid language-region string, make sure defaults to en-US.dll:
  EXPECT_TRUE(loader.GetLocaleFilePath(L"xx", L"XX", &file_path));
  EXPECT_TRUE(file_path.BaseName() == FilePath(L"en-US.dll"));
}
