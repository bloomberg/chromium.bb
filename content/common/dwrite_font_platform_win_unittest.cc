// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/dwrite_font_platform_win.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "content/public/common/content_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/win/direct_write.h"

namespace content {

class DWriteFontCacheTest : public testing::Test {
 public:
  DWriteFontCacheTest() { }

  void SetUp() override {
    base::FilePath data_path;
    ASSERT_TRUE(PathService::Get(content::DIR_TEST_DATA, &data_path));
    arial_cache_path_ =
        data_path.AppendASCII("font/dwrite_font_cache_arial.dat");

    corrupt_cache_path_ =
        data_path.AppendASCII("font/dwrite_font_cache_corrupt.dat");

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    cache_file_path_ = temp_dir_.path().AppendASCII("dwrite_font_cache.dat");
  }

 protected:
    base::FilePath arial_cache_path_;
    base::FilePath corrupt_cache_path_;
    base::ScopedTempDir temp_dir_;
    base::FilePath cache_file_path_;
};

TEST_F(DWriteFontCacheTest, BuildCacheTest) {
  if (gfx::win::ShouldUseDirectWrite()) {
    DLOG(INFO) << __FUNCTION__ << ": " << cache_file_path_.value().c_str();
    EXPECT_TRUE(BuildFontCache(cache_file_path_));
    ASSERT_TRUE(base::PathExists(cache_file_path_));
  }
}

TEST_F(DWriteFontCacheTest, ValidCacheTest) {
  if (gfx::win::ShouldUseDirectWrite()) {
    DLOG(INFO) << __FUNCTION__ << ": " << arial_cache_path_.value().c_str();
    scoped_ptr<base::File> file(new base::File(arial_cache_path_,
                                base::File::FLAG_OPEN | base::File::FLAG_READ));
    ASSERT_TRUE(file->IsValid());

    EXPECT_TRUE(ValidateFontCacheFile(file.get()));
  }
}

TEST_F(DWriteFontCacheTest, CorruptCacheTest) {
  if (gfx::win::ShouldUseDirectWrite()) {
    DLOG(INFO) << __FUNCTION__ << ": " << corrupt_cache_path_.value().c_str();
    scoped_ptr<base::File> file(new base::File(corrupt_cache_path_,
                                base::File::FLAG_OPEN | base::File::FLAG_READ));
    ASSERT_TRUE(file->IsValid());

    EXPECT_FALSE(ValidateFontCacheFile(file.get()));
  }
}

} // content
