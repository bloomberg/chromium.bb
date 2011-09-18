// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/extensions/extension_creator_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class ExtensionCreatorFilterTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_dir_ = temp_dir_.path();

    filter_ = new ExtensionCreatorFilter();
  }

  FilePath CreateTestFile(const FilePath& file_path) {
    FilePath test_file(test_dir_.Append(file_path));
    FilePath temp_file;
    EXPECT_TRUE(file_util::CreateTemporaryFileInDir(test_dir_, &temp_file));
    EXPECT_TRUE(file_util::Move(temp_file, test_file));
    return test_file;
  }

  scoped_refptr<ExtensionCreatorFilter> filter_;

  ScopedTempDir temp_dir_;

  FilePath test_dir_;
};

struct UnaryBooleanTestData {
  const FilePath::CharType* input;
  bool expected;
};

TEST_F(ExtensionCreatorFilterTest, NormalCases) {
  const struct UnaryBooleanTestData cases[] = {
    { FILE_PATH_LITERAL("foo"), true },
    { FILE_PATH_LITERAL(".foo"), false },
    { FILE_PATH_LITERAL(".svn"), false },
    { FILE_PATH_LITERAL("__MACOSX"), false },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath input(cases[i].input);
    FilePath test_file(CreateTestFile(input));
    bool observed = filter_->ShouldPackageFile(test_file);

    EXPECT_EQ(cases[i].expected, observed) <<
      "i: " << i << ", input: " << test_file.value();
  }
}

#if defined(OS_WIN)
struct StringBooleanWithBooleanTestData {
  const FilePath::CharType* input_char;
  bool input_bool;
  bool expected;
};

TEST_F(ExtensionCreatorFilterTest, WindowsHiddenFiles) {
  const struct StringBooleanWithBooleanTestData cases[] = {
    { FILE_PATH_LITERAL("a-normal-file"), false, true },
    { FILE_PATH_LITERAL(".a-dot-file"), false, false },
    { FILE_PATH_LITERAL(".a-dot-file-that-we-have-set-to-hidden"),
      true, false },
    { FILE_PATH_LITERAL("a-file-that-we-have-set-to-hidden"), true, false },
    { FILE_PATH_LITERAL("a-file-that-we-have-not-set-to-hidden"),
      false, true },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath input(cases[i].input_char);
    bool should_hide = cases[i].input_bool;
    FilePath test_file(CreateTestFile(input));

    if (should_hide) {
      SetFileAttributes(test_file.value().c_str(), FILE_ATTRIBUTE_HIDDEN);
    }
    bool observed = filter_->ShouldPackageFile(test_file);
    EXPECT_EQ(cases[i].expected, observed) <<
      "i: " << i << ", input: " << test_file.value();
  }
}
#endif

}  // namespace

