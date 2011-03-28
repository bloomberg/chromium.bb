// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/file_util.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/extension_resource.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

TEST(ExtensionResourceTest, CreateEmptyResource) {
  ExtensionResource resource;

  EXPECT_TRUE(resource.extension_root().empty());
  EXPECT_TRUE(resource.relative_path().empty());
  EXPECT_TRUE(resource.GetFilePath().empty());
}

const FilePath::StringType ToLower(const FilePath::StringType& in_str) {
  FilePath::StringType str(in_str);
  std::transform(str.begin(), str.end(), str.begin(), tolower);
  return str;
}

TEST(ExtensionResourceTest, CreateWithMissingResourceOnDisk) {
  FilePath root_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &root_path));
  FilePath relative_path;
  relative_path = relative_path.AppendASCII("cira.js");
  std::string extension_id;
  Extension::GenerateId("test", &extension_id);
  ExtensionResource resource(extension_id, root_path, relative_path);

  // The path doesn't exist on disk, we will be returned an empty path.
  EXPECT_EQ(root_path.value(), resource.extension_root().value());
  EXPECT_EQ(relative_path.value(), resource.relative_path().value());
  EXPECT_TRUE(resource.GetFilePath().empty());
}

TEST(ExtensionResourceTest, CreateWithAllResourcesOnDisk) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // Create resource in the extension root.
  const char* filename = "res.ico";
  FilePath root_resource = temp.path().AppendASCII(filename);
  std::string data = "some foo";
  ASSERT_TRUE(file_util::WriteFile(root_resource, data.c_str(), data.length()));

  // Create l10n resources (for current locale and its parents).
  FilePath l10n_path = temp.path().Append(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(l10n_path));

  std::vector<std::string> locales;
  l10n_util::GetParentLocales(l10n_util::GetApplicationLocale(""), &locales);
  ASSERT_FALSE(locales.empty());
  for (size_t i = 0; i < locales.size(); i++) {
    FilePath make_path;
    make_path = l10n_path.AppendASCII(locales[i]);
    ASSERT_TRUE(file_util::CreateDirectory(make_path));
    ASSERT_TRUE(file_util::WriteFile(make_path.AppendASCII(filename),
        data.c_str(), data.length()));
  }

  FilePath path;
  std::string extension_id;
  Extension::GenerateId("test", &extension_id);
  ExtensionResource resource(extension_id, temp.path(),
                             FilePath().AppendASCII(filename));
  FilePath resolved_path = resource.GetFilePath();

  FilePath expected_path;
  // Expect default path only, since fallback logic is disabled.
  // See http://crbug.com/27359.
  expected_path = root_resource;
  ASSERT_TRUE(file_util::AbsolutePath(&expected_path));

  EXPECT_EQ(ToLower(expected_path.value()), ToLower(resolved_path.value()));
  EXPECT_EQ(ToLower(temp.path().value()),
            ToLower(resource.extension_root().value()));
  EXPECT_EQ(ToLower(FilePath().AppendASCII(filename).value()),
            ToLower(resource.relative_path().value()));
}
