// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_TEST_RULESET_PUBLISHER_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_TEST_RULESET_PUBLISHER_H_

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

  // Indexes the |unindexed_ruleset| and publishes it to all renderers
  // via the RulesetService. Spins a nested run loop until done.
  void SetRuleset(const TestRuleset& unindexed_ruleset);

 private:

  DISALLOW_COPY_AND_ASSIGN(TestRulesetPublisher);
};

}  // namespace testing
}  // namespace subresource_filter

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_TEST_RULESET_PUBLISHER_H_
