// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/test_ruleset_creator.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "components/subresource_filter/core/common/proto/rules.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

proto::UrlRule CreateSuffixRule(base::StringPiece suffix) {
  proto::UrlRule rule;
  rule.set_semantics(proto::RULE_SEMANTICS_BLACKLIST);
  rule.set_source_type(proto::SOURCE_TYPE_ANY);
  rule.set_element_types(proto::ELEMENT_TYPE_ALL);
  rule.set_url_pattern_type(proto::URL_PATTERN_TYPE_SUBSTRING);
  rule.set_anchor_left(proto::ANCHOR_TYPE_NONE);
  rule.set_anchor_right(proto::ANCHOR_TYPE_BOUNDARY);
  rule.set_url_pattern(suffix.as_string());
  return rule;
}

}  // namespace

namespace testing {

TestRulesetCreator::TestRulesetCreator() = default;
TestRulesetCreator::~TestRulesetCreator() = default;

void TestRulesetCreator::CreateRulesetToDisallowURLsWithPathSuffix(
    base::StringPiece suffix,
    std::vector<uint8_t>* buffer) {
  RulesetIndexer indexer;
  ASSERT_TRUE(indexer.AddUrlRule(CreateSuffixRule(suffix)));
  indexer.Finish();
  buffer->assign(indexer.data(), indexer.data() + indexer.size());
}

void TestRulesetCreator::CreateRulesetFileToDisallowURLsWithPathSuffix(
    base::StringPiece suffix,
    base::File* ruleset_file) {
  DCHECK(ruleset_file);
  ASSERT_TRUE(scoped_temp_dir_.IsValid() ||
              scoped_temp_dir_.CreateUniqueTempDir());
  base::FilePath unique_temp_path = scoped_temp_dir_.path().AppendASCII(
      base::IntToString(next_ruleset_version_++));

  std::vector<uint8_t> ruleset;
  ASSERT_NO_FATAL_FAILURE(
      CreateRulesetToDisallowURLsWithPathSuffix(suffix, &ruleset));

  int ruleset_size = base::checked_cast<int>(ruleset.size());
  static_assert(CHAR_BIT == 8, "Assumed char was 8 bits.");
  ASSERT_EQ(ruleset_size,
            base::WriteFile(unique_temp_path,
                            reinterpret_cast<const char*>(ruleset.data()),
                            ruleset_size));

  ruleset_file->Initialize(unique_temp_path, base::File::FLAG_OPEN |
                                                 base::File::FLAG_READ |
                                                 base::File::FLAG_SHARE_DELETE);
}

}  // namespace testing
}  // namespace subresource_filter
