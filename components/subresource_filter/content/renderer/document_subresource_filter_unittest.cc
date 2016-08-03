// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/document_subresource_filter.h"

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/test_runner/test_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

const char kTestFirstURL[] = "http://example.com/alpha";
const char kTestSecondURL[] = "http://example.com/beta";
const char kTestFirstURLPathSuffix[] = "alpha";

}  // namespace

class DocumentSubresourceFilterTest : public ::testing::Test {
 public:
  DocumentSubresourceFilterTest() { test_runner::EnsureBlinkInitialized(); }

 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  }

  void SetTestRulesetToDisallowURLsWithPathSuffix(base::StringPiece suffix) {
    testing::TestRulesetPair test_ruleset_pair;
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            suffix, &test_ruleset_pair));
    ruleset_ = new MemoryMappedRuleset(
        testing::TestRuleset::Open(test_ruleset_pair.indexed));
  }

  const MemoryMappedRuleset* ruleset() { return ruleset_.get(); }

 private:
  testing::TestRulesetCreator test_ruleset_creator_;
  scoped_refptr<const MemoryMappedRuleset> ruleset_;

  DISALLOW_COPY_AND_ASSIGN(DocumentSubresourceFilterTest);
};

TEST_F(DocumentSubresourceFilterTest, DryRun) {
  blink::WebURLRequest::RequestContext request_context =
      blink::WebURLRequest::RequestContextImage;
  DocumentSubresourceFilter filter(ActivationState::DRYRUN, ruleset(),
                                   std::vector<GURL>());
  EXPECT_TRUE(filter.allowLoad(GURL(kTestFirstURL), request_context));
  EXPECT_TRUE(filter.allowLoad(GURL(kTestSecondURL), request_context));
  EXPECT_EQ(2u, filter.num_loads_total());
  EXPECT_EQ(2u, filter.num_loads_evaluated());
  EXPECT_EQ(1u, filter.num_loads_matching_rules());
  EXPECT_EQ(0u, filter.num_loads_disallowed());
}

TEST_F(DocumentSubresourceFilterTest, Enabled) {
  blink::WebURLRequest::RequestContext request_context =
      blink::WebURLRequest::RequestContextImage;
  DocumentSubresourceFilter filter(ActivationState::ENABLED, ruleset(),
                                   std::vector<GURL>());
  EXPECT_FALSE(filter.allowLoad(GURL(kTestFirstURL), request_context));
  EXPECT_TRUE(filter.allowLoad(GURL(kTestSecondURL), request_context));
  EXPECT_EQ(2u, filter.num_loads_total());
  EXPECT_EQ(2u, filter.num_loads_evaluated());
  EXPECT_EQ(1u, filter.num_loads_matching_rules());
  EXPECT_EQ(1u, filter.num_loads_disallowed());
}

}  // namespace subresource_filter
