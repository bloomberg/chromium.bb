// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_TEST_RULESET_PUBLISHER_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_TEST_RULESET_PUBLISHER_H_

#include <string>

#include "base/macros.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"

namespace subresource_filter {
namespace testing {

// Helper class to create testing rulesets during browser tests, as well as to
// get them indexed and published to renderers by the RulesetService.
class TestRulesetPublisher {
 public:
  TestRulesetPublisher();
  ~TestRulesetPublisher();

  // Creates a testing ruleset to filter subresource loads whose URLs end with
  // the given |suffix|. Then gets this ruleset indexed and published to all
  // renderers via the RulesetService. Spins a nested message loop until done.
  // For simplicity, the |suffix| is also used as the content version of the
  // ruleset, so it must be a valid path component.
  void SetRulesetToDisallowURLsWithPathSuffix(const std::string& suffix);

 private:
  TestRulesetCreator ruleset_creator_;

  DISALLOW_COPY_AND_ASSIGN(TestRulesetPublisher);
};

}  // namespace testing
}  // namespace subresource_filter

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_TEST_RULESET_PUBLISHER_H_
