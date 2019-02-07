// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dwrite_font_lookup_table_builder_win.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/test/scoped_task_environment.h"
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
  DWriteFontLookupTableBuilderTest() = default;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

}  // namespace

// Run a test similar to DWriteFontProxyImplUnitTest, TestFindUniqueFont but
// without going through Mojo and running it on the DWRiteFontLookupTableBuilder
// class directly.
TEST_F(DWriteFontLookupTableBuilderTest, TestFindUniqueFontDirect) {
  DWriteFontLookupTableBuilder* font_lookup_table_builder =
      DWriteFontLookupTableBuilder::GetInstance();
  font_lookup_table_builder->EnsureFontUniqueNameTable();
  base::ReadOnlySharedMemoryRegion font_table_memory =
      font_lookup_table_builder->DuplicatedMemoryRegion();
  blink::FontTableMatcher font_table_matcher(font_table_memory.Map());

  for (auto& test_font_name_index : expected_test_fonts) {
    base::Optional<blink::FontTableMatcher::MatchResult> match_result =
        font_table_matcher.MatchName(test_font_name_index.first);
    CHECK(match_result) << "No font matched for font name: "
                        << test_font_name_index.first;
    base::File unique_font_file(
        base::FilePath::FromUTF8Unsafe(match_result->font_path),
        base::File::FLAG_OPEN | base::File::FLAG_READ);
    CHECK(unique_font_file.IsValid());
    CHECK_GT(unique_font_file.GetLength(), 0);
    CHECK_EQ(test_font_name_index.second, match_result->ttc_index);
  }
}

TEST_F(DWriteFontLookupTableBuilderTest, TestTimeout) {
  DWriteFontLookupTableBuilder* font_lookup_table_builder =
      DWriteFontLookupTableBuilder::GetInstance();
  font_lookup_table_builder->ResetLookupTableForTesting();
  font_lookup_table_builder->SetSlowDownIndexingForTesting(true);
  font_lookup_table_builder->EnsureFontUniqueNameTable();
  base::ReadOnlySharedMemoryRegion font_table_memory =
      font_lookup_table_builder->DuplicatedMemoryRegion();
  blink::FontTableMatcher font_table_matcher(font_table_memory.Map());

  for (auto& test_font_name_index : expected_test_fonts) {
    base::Optional<blink::FontTableMatcher::MatchResult> match_result =
        font_table_matcher.MatchName(test_font_name_index.first);
    CHECK(!match_result);
  }
  // Need to reset the table again so that it can be rebuilt successfully
  // without the artificial timeout when running the next test.
  font_lookup_table_builder->ResetLookupTableForTesting();
}

}  // namespace content
