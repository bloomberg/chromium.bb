// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/extensions/extension_test_util.h"
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
  std::string extension_id = extension_test_util::MakeId("test");
  ExtensionResource resource(extension_id, root_path, relative_path);

  // The path doesn't exist on disk, we will be returned an empty path.
  EXPECT_EQ(root_path.value(), resource.extension_root().value());
  EXPECT_EQ(relative_path.value(), resource.relative_path().value());
  EXPECT_TRUE(resource.GetFilePath().empty());
}

TEST(ExtensionResourceTest, ResourcesOutsideOfPath) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath inner_dir = temp.path().AppendASCII("directory");
  ASSERT_TRUE(file_util::CreateDirectory(inner_dir));
  FilePath inner_file = inner_dir.AppendASCII("inner");
  FilePath outer_file = temp.path().AppendASCII("outer");
  ASSERT_TRUE(file_util::WriteFile(outer_file, "X", 1));
  ASSERT_TRUE(file_util::WriteFile(inner_file, "X", 1));
  std::string extension_id = extension_test_util::MakeId("test");

#if defined(OS_POSIX)
  FilePath symlink_file = inner_dir.AppendASCII("symlink");
  file_util::CreateSymbolicLink(
      FilePath().AppendASCII("..").AppendASCII("outer"),
      symlink_file);
#endif

  // A non-packing extension should be able to access the file within the
  // directory.
  ExtensionResource r1(extension_id, inner_dir,
                       FilePath().AppendASCII("inner"));
  EXPECT_FALSE(r1.GetFilePath().empty());

  // ... but not a relative path that walks out of |inner_dir|.
  ExtensionResource r2(extension_id, inner_dir,
                       FilePath().AppendASCII("..").AppendASCII("outer"));
  EXPECT_TRUE(r2.GetFilePath().empty());

  // A packing extension should also be able to access the file within the
  // directory.
  ExtensionResource r3(extension_id, inner_dir,
                       FilePath().AppendASCII("inner"));
  r3.set_follow_symlinks_anywhere();
  EXPECT_FALSE(r3.GetFilePath().empty());

  // ... but, again, not a relative path that walks out of |inner_dir|.
  ExtensionResource r4(extension_id, inner_dir,
                       FilePath().AppendASCII("..").AppendASCII("outer"));
  r4.set_follow_symlinks_anywhere();
  EXPECT_TRUE(r4.GetFilePath().empty());

#if defined(OS_POSIX)
  // The non-packing extension should also not be able to access a resource that
  // symlinks out of the directory.
  ExtensionResource r5(extension_id, inner_dir,
                       FilePath().AppendASCII("symlink"));
  EXPECT_TRUE(r5.GetFilePath().empty());

  // ... but a packing extension can.
  ExtensionResource r6(extension_id, inner_dir,
                       FilePath().AppendASCII("symlink"));
  r6.set_follow_symlinks_anywhere();
  EXPECT_FALSE(r6.GetFilePath().empty());
#endif
}

// crbug.com/108721. Disabled on Windows due to crashing on Vista.
#if defined(OS_WIN)
#define CreateWithAllResourcesOnDisk DISABLED_CreateWithAllResourcesOnDisk
#endif
TEST(ExtensionResourceTest, CreateWithAllResourcesOnDisk) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // Create resource in the extension root.
  const char* filename = "res.ico";
  FilePath root_resource = temp.path().AppendASCII(filename);
  std::string data = "some foo";
  ASSERT_TRUE(file_util::WriteFile(root_resource, data.c_str(), data.length()));

  // Create l10n resources (for current locale and its parents).
  FilePath l10n_path = temp.path().Append(extensions::Extension::kLocaleFolder);
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
  std::string extension_id = extension_test_util::MakeId("test");
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
