// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/test_ruleset_creator.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {
namespace testing {

TestRulesetCreator::TestRulesetCreator() = default;
TestRulesetCreator::~TestRulesetCreator() = default;

void TestRulesetCreator::CreateRulesetToDisallowURLsWithPathSuffix(
    base::StringPiece suffix,
    base::File* ruleset_file) {
  DCHECK(ruleset_file);
  ASSERT_TRUE(scoped_temp_dir_.IsValid() ||
              scoped_temp_dir_.CreateUniqueTempDir());
  base::FilePath unique_temp_path = scoped_temp_dir_.path().AppendASCII(
      base::IntToString(next_ruleset_version_++));
  int ruleset_size = base::checked_cast<int>(suffix.size());
  ASSERT_EQ(ruleset_size,
            base::WriteFile(unique_temp_path, suffix.data(), ruleset_size));
  ruleset_file->Initialize(unique_temp_path, base::File::FLAG_OPEN |
                                                 base::File::FLAG_READ |
                                                 base::File::FLAG_SHARE_DELETE);
}

}  // namespace testing
}  // namespace subresource_filter
