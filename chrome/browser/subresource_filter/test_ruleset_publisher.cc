// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/test_ruleset_publisher.h"

#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "components/subresource_filter/content/browser/content_ruleset_service_delegate.h"
#include "components/subresource_filter/core/browser/ruleset_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {
namespace testing {

namespace {

class RulesetDistributionListener {
 public:
  RulesetDistributionListener()
      : delegate_(static_cast<ContentRulesetServiceDelegate*>(
            g_browser_process->subresource_filter_ruleset_service()
                ->delegate())) {
    delegate_->SetRulesetPublishedCallbackForTesting(run_loop_.QuitClosure());
  }

  ~RulesetDistributionListener() {
    delegate_->SetRulesetPublishedCallbackForTesting(base::Closure());
  }

  void AwaitDistribution() { run_loop_.Run(); }

 private:
  ContentRulesetServiceDelegate* delegate_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(RulesetDistributionListener);
};

}  // namespace

TestRulesetPublisher::TestRulesetPublisher() = default;
TestRulesetPublisher::~TestRulesetPublisher() = default;

void TestRulesetPublisher::SetRulesetToDisallowURLsWithPathSuffix(
    const std::string& suffix) {
  const std::string& test_ruleset_content_version(suffix);
  TestRulesetPair test_ruleset_pair;
  ASSERT_NO_FATAL_FAILURE(
      ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
          suffix, &test_ruleset_pair))
      << suffix;
  subresource_filter::UnindexedRulesetInfo unindexed_ruleset_info;
  unindexed_ruleset_info.content_version = test_ruleset_content_version;
  unindexed_ruleset_info.ruleset_path = test_ruleset_pair.unindexed.path;
  RulesetDistributionListener listener;
  g_browser_process->subresource_filter_ruleset_service()
      ->IndexAndStoreAndPublishRulesetIfNeeded(unindexed_ruleset_info);
  listener.AwaitDistribution();
}

}  // namespace testing
}  // namespace subresource_filter
