// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/simple_resource_loader.h"

#include "base/files/file_path.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/data_pack.h"

TEST(SimpleResourceLoaderTest, LoadLocaleDll) {
  std::vector<std::wstring> language_tags;
  base::FilePath locales_path;
  base::FilePath file_path;
  HMODULE dll_handle = NULL;

  SimpleResourceLoader::DetermineLocalesDirectory(&locales_path);

  // Test valid language-region string:
  language_tags.clear();
  language_tags.push_back(L"en-GB");
  language_tags.push_back(L"en");
  ui::DataPack* data_pack = NULL;

  std::wstring language;

  EXPECT_TRUE(
      SimpleResourceLoader::LoadLocalePack(language_tags, locales_path,
                                           &dll_handle, &data_pack,
                                           &language));
  if (NULL != dll_handle) {
    FreeLibrary(dll_handle);
    dll_handle = NULL;
  }
  EXPECT_TRUE(data_pack != NULL);
  delete data_pack;
  data_pack = NULL;

  EXPECT_EQ(language, L"en-GB");
  language.clear();

  // Test valid language-region string for which we only have a language dll:
  language_tags.clear();
  language_tags.push_back(L"fr-FR");
  language_tags.push_back(L"fr");
  EXPECT_TRUE(
      SimpleResourceLoader::LoadLocalePack(language_tags, locales_path,
                                           &dll_handle, &data_pack,
                                           &language));
  if (NULL != dll_handle) {
    FreeLibrary(dll_handle);
    dll_handle = NULL;
  }
  EXPECT_TRUE(data_pack != NULL);
  delete data_pack;
  data_pack = NULL;

  EXPECT_EQ(language, L"fr");
  language.clear();

  // Test invalid language-region string, make sure fallback works:
  language_tags.clear();
  language_tags.push_back(L"xx-XX");
  language_tags.push_back(L"en-US");
  EXPECT_TRUE(
      SimpleResourceLoader::LoadLocalePack(language_tags, locales_path,
                                           &dll_handle, &data_pack,
                                           &language));
  if (NULL != dll_handle) {
    FreeLibrary(dll_handle);
    dll_handle = NULL;
  }

  EXPECT_TRUE(data_pack != NULL);
  delete data_pack;
  data_pack = NULL;

  EXPECT_EQ(language, L"en-US");
  language.clear();
}

TEST(SimpleResourceLoaderTest, InstanceTest) {
  SimpleResourceLoader* loader = SimpleResourceLoader::GetInstance();

  ASSERT_TRUE(NULL != loader);
  ASSERT_TRUE(NULL != loader->GetResourceModuleHandle());
}
