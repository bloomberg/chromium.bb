// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dwrite_font_lookup_table_builder_win.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"

namespace content {

namespace {

std::vector<std::pair<std::string, uint32_t>> expected_test_fonts = {
    {u8"CambriaMath", 1},
    {u8"Ming-Lt-HKSCS-ExtB", 2},
    {u8"NSimSun", 1},
    {u8"calibri-bolditalic", 0}};

class DWriteFontLookupTableBuilderTest : public testing::Test {
 public:
  DWriteFontLookupTableBuilderTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::ExecutionMode::ASYNC) {
    feature_list_.InitAndEnableFeature(features::kFontSrcLocalMatching);
  }

  void SetUp() override {
    font_lookup_table_builder_ = DWriteFontLookupTableBuilder::GetInstance();
    font_lookup_table_builder_->ResetLookupTableForTesting();
    DCHECK(scoped_temp_dir_.CreateUniqueTempDir());
    font_lookup_table_builder_->SetCacheDirectoryForTesting(
        scoped_temp_dir_.GetPath());
  }

 protected:
  DWriteFontLookupTableBuilder* font_lookup_table_builder_;

  void TestMatchFonts() {
    base::ReadOnlySharedMemoryRegion font_table_memory =
        font_lookup_table_builder_->DuplicateMemoryRegion();
    blink::FontTableMatcher font_table_matcher(font_table_memory.Map());

    for (auto& test_font_name_index : expected_test_fonts) {
      base::Optional<blink::FontTableMatcher::MatchResult> match_result =
          font_table_matcher.MatchName(test_font_name_index.first);
      ASSERT_TRUE(match_result)
          << "No font matched for font name: " << test_font_name_index.first;
      base::File unique_font_file(
          base::FilePath::FromUTF8Unsafe(match_result->font_path),
          base::File::FLAG_OPEN | base::File::FLAG_READ);
      ASSERT_TRUE(unique_font_file.IsValid());
      ASSERT_GT(unique_font_file.GetLength(), 0);
      ASSERT_EQ(test_font_name_index.second, match_result->ttc_index);
    }
  }
  base::ScopedTempDir scoped_temp_dir_;

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

class DWriteFontLookupTableBuilderTimeoutTest
    : public DWriteFontLookupTableBuilderTest,
      public ::testing::WithParamInterface<
          DWriteFontLookupTableBuilder::SlowDownMode> {};

}  // namespace

// Run a test similar to DWriteFontProxyImplUnitTest, TestFindUniqueFont but
// without going through Mojo and running it on the DWRiteFontLookupTableBuilder
// class directly.
TEST_F(DWriteFontLookupTableBuilderTest, TestFindUniqueFontDirect) {
  font_lookup_table_builder_->SchedulePrepareFontUniqueNameTable();
  font_lookup_table_builder_->EnsureFontUniqueNameTable();
  TestMatchFonts();
}

TEST_P(DWriteFontLookupTableBuilderTimeoutTest, TestTimeout) {
  font_lookup_table_builder_->SetSlowDownIndexingForTesting(GetParam());
  font_lookup_table_builder_->SchedulePrepareFontUniqueNameTable();
  font_lookup_table_builder_->EnsureFontUniqueNameTable();
  base::ReadOnlySharedMemoryRegion font_table_memory =
      font_lookup_table_builder_->DuplicateMemoryRegion();
  blink::FontTableMatcher font_table_matcher(font_table_memory.Map());

  for (auto& test_font_name_index : expected_test_fonts) {
    base::Optional<blink::FontTableMatcher::MatchResult> match_result =
        font_table_matcher.MatchName(test_font_name_index.first);
    ASSERT_TRUE(!match_result);
  }
  if (GetParam() == DWriteFontLookupTableBuilder::SlowDownMode::kHangOneTask)
    font_lookup_table_builder_->ResumeFromHangForTesting();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    DWriteFontLookupTableBuilderTimeoutTest,
    ::testing::Values(
        DWriteFontLookupTableBuilder::SlowDownMode::kDelayEachTask,
        DWriteFontLookupTableBuilder::SlowDownMode::kHangOneTask));

TEST_F(DWriteFontLookupTableBuilderTest, RepeatedScheduling) {
  for (unsigned i = 0; i < 3; ++i) {
    font_lookup_table_builder_->ResetLookupTableForTesting();
    font_lookup_table_builder_->SetCachingEnabledForTesting(false);
    font_lookup_table_builder_->SchedulePrepareFontUniqueNameTable();
    font_lookup_table_builder_->EnsureFontUniqueNameTable();
  }
}

TEST_F(DWriteFontLookupTableBuilderTest, FontsHash) {
  ASSERT_GT(font_lookup_table_builder_->ComputePersistenceHash().size(), 0u);
}

TEST_F(DWriteFontLookupTableBuilderTest, HandleCorruptCacheFile) {
  // Cycle once to build cache file.
  font_lookup_table_builder_->ResetLookupTableForTesting();
  font_lookup_table_builder_->SchedulePrepareFontUniqueNameTable();
  font_lookup_table_builder_->EnsureFontUniqueNameTable();
  // Truncate table for testing
  base::FilePath cache_file_path = scoped_temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("font_unique_name_table.pb"));
  // Use FLAG_EXCLUSIVE_WRITE to block file and make persisting the cache fail
  // as well.
  base::File cache_file(cache_file_path, base::File::FLAG_OPEN |
                                             base::File::FLAG_READ |
                                             base::File::FLAG_WRITE |
                                             base::File::FLAG_EXCLUSIVE_WRITE);
  ASSERT_TRUE(cache_file.IsValid());
  ASSERT_TRUE(cache_file.SetLength(cache_file.GetLength() / 2));
  ASSERT_TRUE(cache_file.SetLength(cache_file.GetLength() * 2));

  // Reload the cache file.
  font_lookup_table_builder_->ResetLookupTableForTesting();
  font_lookup_table_builder_->SchedulePrepareFontUniqueNameTable();
  ASSERT_TRUE(font_lookup_table_builder_->EnsureFontUniqueNameTable());

  TestMatchFonts();

  // Ensure that the table is still valid even though persisting has failed due
  // to the exclusive write lock on the file.
  ASSERT_TRUE(font_lookup_table_builder_->EnsureFontUniqueNameTable());
}

}  // namespace content
