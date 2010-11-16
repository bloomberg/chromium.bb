// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/simple_resource_loader.h"

#include "base/file_path.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SimpleResourceLoaderTest, LoadLocaleDll) {
  std::vector<std::wstring> language_tags;
  FilePath locales_path;
  FilePath file_path;
  HMODULE dll_handle = NULL;

  SimpleResourceLoader::DetermineLocalesDirectory(&locales_path);

  // Test valid language-region string:
  language_tags.clear();
  language_tags.push_back(L"en-GB");
  language_tags.push_back(L"en");
  EXPECT_TRUE(
      SimpleResourceLoader::LoadLocaleDll(language_tags, locales_path,
                                          &dll_handle, &file_path));
  if (NULL != dll_handle) {
    FreeLibrary(dll_handle);
    dll_handle = NULL;
  }
  EXPECT_TRUE(file_path.BaseName() == FilePath(L"en-GB.dll"));

  // Test valid language-region string for which we only have a language dll:
  language_tags.clear();
  language_tags.push_back(L"fr-FR");
  language_tags.push_back(L"fr");
  EXPECT_TRUE(
      SimpleResourceLoader::LoadLocaleDll(language_tags, locales_path,
                                          &dll_handle, &file_path));
  if (NULL != dll_handle) {
    FreeLibrary(dll_handle);
    dll_handle = NULL;
  }
  EXPECT_TRUE(file_path.BaseName() == FilePath(L"fr.dll"));

  // Test invalid language-region string, make sure fallback works:
  language_tags.clear();
  language_tags.push_back(L"xx-XX");
  language_tags.push_back(L"en-US");
  EXPECT_TRUE(
      SimpleResourceLoader::LoadLocaleDll(language_tags, locales_path,
                                          &dll_handle, &file_path));
  if (NULL != dll_handle) {
    FreeLibrary(dll_handle);
    dll_handle = NULL;
  }
  EXPECT_TRUE(file_path.BaseName() == FilePath(L"en-US.dll"));
}

TEST(SimpleResourceLoaderTest, InstanceTest) {
  SimpleResourceLoader* loader = SimpleResourceLoader::instance();

  ASSERT_TRUE(NULL != loader);
  ASSERT_TRUE(NULL != loader->GetResourceModuleHandle());
}
