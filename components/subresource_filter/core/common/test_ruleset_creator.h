// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_CREATOR_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_CREATOR_H_

#include <stdint.h>

#include <vector>

#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"

namespace subresource_filter {
namespace testing {

// Helper class to create subresource filtering rulesets for testing.
class TestRulesetCreator {
 public:
  TestRulesetCreator();
  ~TestRulesetCreator();

  // Creates a testing ruleset to disallow subresource loads from URL paths
  // having the given |suffix|, and replaces the |buffer| with the ruleset data.
  //
  // Enclose the call to this method in ASSERT_NO_FATAL_FAILURE to detect
  // errors.
  static void CreateRulesetToDisallowURLsWithPathSuffix(
      base::StringPiece suffix,
      std::vector<uint8_t>* buffer);

  // Same as above, but puts the ruleset into a file and returns its read-only
  // file handle in |ruleset_file|.
  //
  // The underlying temporary file will be deleted when the TestRulesetCreator
  // instance goes out of scope, but the |ruleset_file| handle may outlive and
  // can still be used even after the destruction.
  //
  // Enclose the call to this method in ASSERT_NO_FATAL_FAILURE to detect
  // errors.
  void CreateRulesetFileToDisallowURLsWithPathSuffix(base::StringPiece suffix,
                                                     base::File* ruleset_file);

 private:
  base::ScopedTempDir scoped_temp_dir_;
  int next_ruleset_version_ = 1;

  DISALLOW_COPY_AND_ASSIGN(TestRulesetCreator);
};

}  // namespace testing
}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_CREATOR_H_
