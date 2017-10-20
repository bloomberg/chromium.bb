// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/feature_policy/feature_policy.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

blink::WebFeaturePolicyFeature kDefaultOnFeature =
    static_cast<blink::WebFeaturePolicyFeature>(
        static_cast<int>(blink::WebFeaturePolicyFeature::LAST_FEATURE) + 1);

blink::WebFeaturePolicyFeature kDefaultSelfFeature =
    static_cast<blink::WebFeaturePolicyFeature>(
        static_cast<int>(blink::WebFeaturePolicyFeature::LAST_FEATURE) + 2);

blink::WebFeaturePolicyFeature kDefaultOffFeature =
    static_cast<blink::WebFeaturePolicyFeature>(
        static_cast<int>(blink::WebFeaturePolicyFeature::LAST_FEATURE) + 3);

}  // namespace

class FeaturePolicyTest : public ::testing::Test {
 protected:
  FeaturePolicyTest()
      : feature_list_(
            {{kDefaultOnFeature, FeaturePolicy::FeatureDefault::EnableForAll},
             {kDefaultSelfFeature,
              FeaturePolicy::FeatureDefault::EnableForSelf},
             {kDefaultOffFeature,
              FeaturePolicy::FeatureDefault::DisableForAll}}) {}

  ~FeaturePolicyTest() override {}

  std::unique_ptr<FeaturePolicy> CreateFromParentPolicy(
      const FeaturePolicy* parent,
      const url::Origin& origin) {
    ParsedFeaturePolicyHeader empty_container_policy;
    return FeaturePolicy::CreateFromParentPolicy(parent, empty_container_policy,
                                                 origin, feature_list_);
  }

  std::unique_ptr<FeaturePolicy> CreateFromParentWithFramePolicy(
      const FeaturePolicy* parent,
      const ParsedFeaturePolicyHeader& frame_policy,
      const url::Origin& origin) {
    return FeaturePolicy::CreateFromParentPolicy(parent, frame_policy, origin,
                                                 feature_list_);
  }
  url::Origin origin_a_ = url::Origin::Create(GURL("https://example.com/"));
  url::Origin origin_b_ = url::Origin::Create(GURL("https://example.net/"));
  url::Origin origin_c_ = url::Origin::Create(GURL("https://example.org/"));

 private:
  // Contains the list of controlled features, so that we are guaranteed to
  // have at least one of each kind of default behaviour represented.
  FeaturePolicy::FeatureList feature_list_;
};

TEST_F(FeaturePolicyTest, TestInitialPolicy) {
  // +-------------+
  // |(1)Origin A  |
  // |No Policy    |
  // +-------------+
  // Default-on and top-level-only features should be enabled in top-level
  // frame. Default-off features should be disabled.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy1->IsFeatureEnabled(kDefaultOffFeature));
}

TEST_F(FeaturePolicyTest, TestInitialSameOriginChildPolicy) {
  // +-----------------+
  // |(1)Origin A      |
  // |No Policy        |
  // | +-------------+ |
  // | |(2)Origin A  | |
  // | |No Policy    | |
  // | +-------------+ |
  // +-----------------+
  // Default-on and Default-self features should be enabled in a same-origin
  // child frame. Default-off features should be disabled.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultOffFeature));
}

TEST_F(FeaturePolicyTest, TestInitialCrossOriginChildPolicy) {
  // +-----------------+
  // |(1)Origin A      |
  // |No Policy        |
  // | +-------------+ |
  // | |(2)Origin B  | |
  // | |No Policy    | |
  // | +-------------+ |
  // +-----------------+
  // Default-on features should be enabled in child frame. Default-self and
  // Default-off features should be disabled.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultOffFeature));
}

TEST_F(FeaturePolicyTest, TestCrossOriginChildCannotEnableFeature) {
  // +---------------------------------------+
  // |(1) Origin A                           |
  // |No Policy                              |
  // | +-----------------------------------+ |
  // | |(2) Origin B                       | |
  // | |Policy: {"default-self": ["self"]} | |
  // | +-----------------------------------+ |
  // +---------------------------------------+
  // Default-self feature should be disabled in cross origin frame, even if no
  // policy was specified in the parent frame.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  policy2->SetHeaderPolicy({{{kDefaultSelfFeature, false, {origin_b_}}}});
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestFrameSelfInheritance) {
  // +------------------------------------------+
  // |(1) Origin A                              |
  // |Policy: {"default-self": ["self"]}        |
  // | +-----------------+  +-----------------+ |
  // | |(2) Origin A     |  |(4) Origin B     | |
  // | |No Policy        |  |No Policy        | |
  // | | +-------------+ |  | +-------------+ | |
  // | | |(3)Origin A  | |  | |(5)Origin B  | | |
  // | | |No Policy    | |  | |No Policy    | | |
  // | | +-------------+ |  | +-------------+ | |
  // | +-----------------+  +-----------------+ |
  // +------------------------------------------+
  // Feature should be enabled at the top-level, and through the chain of
  // same-origin frames 2 and 3. It should be disabled in frames 4 and 5, as
  // they are at a different origin.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy({{{kDefaultSelfFeature, false, {origin_a_}}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy5 =
      CreateFromParentPolicy(policy4.get(), origin_b_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy5->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestReflexiveFrameSelfInheritance) {
  // +-----------------------------------+
  // |(1) Origin A                       |
  // |Policy: {"default-self": ["self"]} |
  // | +-----------------+               |
  // | |(2) Origin B     |               |
  // | |No Policy        |               |
  // | | +-------------+ |               |
  // | | |(3)Origin A  | |               |
  // | | |No Policy    | |               |
  // | | +-------------+ |               |
  // | +-----------------+               |
  // +-----------------------------------+
  // Feature which is enabled at top-level should be disabled in frame 3, as
  // it is embedded by frame 2, for which the feature is not enabled.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy({{{kDefaultSelfFeature, false, {origin_a_}}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestSelectiveFrameInheritance) {
  // +------------------------------------------+
  // |(1) Origin A                              |
  // |Policy: {"default-self": ["Origin B"]}    |
  // | +-----------------+  +-----------------+ |
  // | |(2) Origin B     |  |(3) Origin C     | |
  // | |No Policy        |  |No Policy        | |
  // | |                 |  | +-------------+ | |
  // | |                 |  | |(4)Origin B  | | |
  // | |                 |  | |No Policy    | | |
  // | |                 |  | +-------------+ | |
  // | +-----------------+  +-----------------+ |
  // +------------------------------------------+
  // Feature should be enabled in second level Origin B frame, but disabled in
  // Frame 4, because it is embedded by frame 3, where the feature is not
  // enabled.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy({{{kDefaultSelfFeature, false, {origin_b_}}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy1.get(), origin_c_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy3.get(), origin_b_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestPolicyCanBlockSelf) {
  // +----------------------------+
  // |(1)Origin A                 |
  // |Policy: {"default-on": []}  |
  // +----------------------------+
  // Default-on feature should be disabled in top-level frame.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultOnFeature, false, std::vector<url::Origin>()}}});
  EXPECT_FALSE(policy1->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestParentPolicyBlocksSameOriginChildPolicy) {
  // +----------------------------+
  // |(1)Origin A                 |
  // |Policy: {"default-on": []}  |
  // | +-------------+            |
  // | |(2)Origin A  |            |
  // | |No Policy    |            |
  // | +-------------+            |
  // +----------------------------+
  // Feature should be disabled in child frame.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultOnFeature, false, std::vector<url::Origin>()}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestChildPolicyCanBlockSelf) {
  // +--------------------------------+
  // |(1)Origin A                     |
  // |No Policy                       |
  // | +----------------------------+ |
  // | |(2)Origin B                 | |
  // | |Policy: {"default-on": []}  | |
  // | +----------------------------+ |
  // +--------------------------------+
  // Default-on feature should be disabled by cross-origin child frame.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  policy2->SetHeaderPolicy(
      {{{kDefaultOnFeature, false, std::vector<url::Origin>()}}});
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestChildPolicyCanBlockChildren) {
  // +--------------------------------------+
  // |(1)Origin A                           |
  // |No Policy                             |
  // | +----------------------------------+ |
  // | |(2)Origin B                       | |
  // | |Policy: {"default-on": ["self"]}  | |
  // | | +-------------+                  | |
  // | | |(3)Origin C  |                  | |
  // | | |No Policy    |                  | |
  // | | +-------------+                  | |
  // | +----------------------------------+ |
  // +--------------------------------------+
  // Default-on feature should be enabled in frames 1 and 2; disabled in frame
  // 3 by child frame policy.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  policy2->SetHeaderPolicy({{{kDefaultOnFeature, false, {origin_b_}}}});
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestParentPolicyBlocksCrossOriginChildPolicy) {
  // +----------------------------+
  // |(1)Origin A                 |
  // |Policy: {"default-on": []}  |
  // | +-------------+            |
  // | |(2)Origin B  |            |
  // | |No Policy    |            |
  // | +-------------+            |
  // +----------------------------+
  // Default-on feature should be disabled in cross-origin child frame.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultOnFeature, false, std::vector<url::Origin>()}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestEnableForAllOrigins) {
  // +--------------------------------+
  // |(1) Origin A                    |
  // |Policy: {"default-self": ["*"]} |
  // | +-----------------+            |
  // | |(2) Origin B     |            |
  // | |No Policy        |            |
  // | | +-------------+ |            |
  // | | |(3)Origin A  | |            |
  // | | |No Policy    | |            |
  // | | +-------------+ |            |
  // | +-----------------+            |
  // +--------------------------------+
  // Feature should be enabled in top and second level; disabled in frame 3.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, true, std::vector<url::Origin>()}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestDefaultOnEnablesForAllAncestors) {
  // +---------------------------------------+
  // |(1) Origin A                           |
  // |Policy: {"default-on": ["Origin B"]}   |
  // | +-----------------------------------+ |
  // | |(2) Origin B                       | |
  // | |No Policy                          | |
  // | | +-------------+   +-------------+ | |
  // | | |(3)Origin B  |   |(4)Origin C  | | |
  // | | |No Policy    |   |No Policy    | | |
  // | | +-------------+   +-------------+ | |
  // | +-----------------------------------+ |
  // +---------------------------------------+
  // Feature should be disabled in frame 1; enabled in frames 2, 3 and 4.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy({{{kDefaultOnFeature, false, {origin_b_}}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_FALSE(policy1->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy4->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestDefaultSelfRespectsSameOriginEmbedding) {
  // +---------------------------------------+
  // |(1) Origin A                           |
  // |Policy: {"default-self": ["Origin B"]} |
  // | +-----------------------------------+ |
  // | |(2) Origin B                       | |
  // | |No Policy                          | |
  // | | +-------------+   +-------------+ | |
  // | | |(3)Origin B  |   |(4)Origin C  | | |
  // | | |No Policy    |   |No Policy    | | |
  // | | +-------------+   +-------------+ | |
  // | +-----------------------------------+ |
  // +---------------------------------------+
  // Feature should be disabled in frames 1 and 4; enabled in frames 2 and 3.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy({{{kDefaultSelfFeature, false, {origin_b_}}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_FALSE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestDefaultOffMustBeDelegatedToAllCrossOriginFrames) {
  // +------------------------------------------------------------+
  // |(1) Origin A                                                |
  // |Policy: {"default-off": ["Origin B"]}                       |
  // | +--------------------------------------------------------+ |
  // | |(2) Origin B                                            | |
  // | |Policy: {"default-off": ["self"]}                       | |
  // | | +-------------+   +----------------------------------+ | |
  // | | |(3)Origin B  |   |(4)Origin C                       | | |
  // | | |No Policy    |   |Policy: {"default-off": ["self"]} | | |
  // | | +-------------+   +----------------------------------+ | |
  // | +--------------------------------------------------------+ |
  // +------------------------------------------------------------+
  // Feature should be disabled in frames 1, 3 and 4; enabled in frame 2 only.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy({{{kDefaultOffFeature, false, {origin_b_}}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  policy2->SetHeaderPolicy({{{kDefaultOffFeature, false, {origin_b_}}}});
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  policy4->SetHeaderPolicy({{{kDefaultOffFeature, false, {origin_c_}}}});
  EXPECT_FALSE(policy1->IsFeatureEnabled(kDefaultOffFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOffFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultOffFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultOffFeature));
}

TEST_F(FeaturePolicyTest, TestReenableForAllOrigins) {
  // +------------------------------------+
  // |(1) Origin A                        |
  // |Policy: {"default-self": ["*"]}     |
  // | +--------------------------------+ |
  // | |(2) Origin B                    | |
  // | |Policy: {"default-self": ["*"]} | |
  // | | +-------------+                | |
  // | | |(3)Origin A  |                | |
  // | | |No Policy    |                | |
  // | | +-------------+                | |
  // | +--------------------------------+ |
  // +------------------------------------+
  // Feature should be enabled in all frames.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, true, std::vector<url::Origin>()}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  policy2->SetHeaderPolicy(
      {{{kDefaultSelfFeature, true, std::vector<url::Origin>()}}});
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestBlockedFrameCannotReenable) {
  // +--------------------------------------+
  // |(1)Origin A                           |
  // |Policy: {"default-self": ["self"]}    |
  // | +----------------------------------+ |
  // | |(2)Origin B                       | |
  // | |Policy: {"default-self": ["*"]}   | |
  // | | +-------------+  +-------------+ | |
  // | | |(3)Origin A  |  |(4)Origin C  | | |
  // | | |No Policy    |  |No Policy    | | |
  // | | +-------------+  +-------------+ | |
  // | +----------------------------------+ |
  // +--------------------------------------+
  // Feature should be enabled at the top level; disabled in all other frames.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy({{{kDefaultSelfFeature, false, {origin_a_}}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  policy2->SetHeaderPolicy(
      {{{kDefaultSelfFeature, true, std::vector<url::Origin>()}}});
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestEnabledFrameCanDelegate) {
  // +---------------------------------------------------+
  // |(1) Origin A                                       |
  // |Policy: {"default-self": ["self", "Origin B"]}     |
  // | +-----------------------------------------------+ |
  // | |(2) Origin B                                   | |
  // | |Policy: {"default-self": ["self", "Origin C"]} | |
  // | | +-------------+                               | |
  // | | |(3)Origin C  |                               | |
  // | | |No Policy    |                               | |
  // | | +-------------+                               | |
  // | +-----------------------------------------------+ |
  // +---------------------------------------------------+
  // Feature should be enabled in all frames.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, false, {origin_a_, origin_b_}}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  policy2->SetHeaderPolicy(
      {{{kDefaultSelfFeature, false, {origin_b_, origin_c_}}}});
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestEnabledFrameCanDelegateByDefault) {
  // +-----------------------------------------------+
  // |(1) Origin A                                   |
  // |Policy: {"default-on": ["self", "Origin B"]}   |
  // | +--------------------+ +--------------------+ |
  // | |(2) Origin B        | | (4) Origin C       | |
  // | |No Policy           | | No Policy          | |
  // | | +-------------+    | |                    | |
  // | | |(3)Origin C  |    | |                    | |
  // | | |No Policy    |    | |                    | |
  // | | +-------------+    | |                    | |
  // | +--------------------+ +--------------------+ |
  // +-----------------------------------------------+
  // Feature should be enabled in frames 1, 2, and 3, and disabled in frame 4.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultOnFeature, false, {origin_a_, origin_b_}}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy1.get(), origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestNonNestedFeaturesDontDelegateByDefault) {
  // +-----------------------------------------------+
  // |(1) Origin A                                   |
  // |Policy: {"default-self": ["self", "Origin B"]} |
  // | +--------------------+ +--------------------+ |
  // | |(2) Origin B        | | (4) Origin C       | |
  // | |No Policy           | | No Policy          | |
  // | | +-------------+    | |                    | |
  // | | |(3)Origin C  |    | |                    | |
  // | | |No Policy    |    | |                    | |
  // | | +-------------+    | |                    | |
  // | +--------------------+ +--------------------+ |
  // +-----------------------------------------------+
  // Feature should be enabled in frames 1 and 2, and disabled in frames 3 and
  // 4.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, false, {origin_a_, origin_b_}}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy1.get(), origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestFeaturesAreIndependent) {
  // +-----------------------------------------------+
  // |(1) Origin A                                   |
  // |Policy: {"default-self": ["self", "Origin B"], |
  // |         "default-on": ["self"]}               |
  // | +-------------------------------------------+ |
  // | |(2) Origin B                               | |
  // | |Policy: {"default-self": ["*"],            | |
  // | |         "default-on": ["*"]}              | |
  // | | +-------------+                           | |
  // | | |(3)Origin C  |                           | |
  // | | |No Policy    |                           | |
  // | | +-------------+                           | |
  // | +-------------------------------------------+ |
  // +-----------------------------------------------+
  // Default-self feature should be enabled in all frames; Default-on feature
  // should be enabled in frame 1, and disabled in frames 2 and 3.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, false, {origin_a_, origin_b_}},
        {kDefaultOnFeature, false, {origin_a_}}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  policy2->SetHeaderPolicy(
      {{{kDefaultSelfFeature, true, std::vector<url::Origin>()},
        {kDefaultOnFeature, true, std::vector<url::Origin>()}}});
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestFeatureEnabledForOrigin) {
  // +-----------------------------------------------+
  // |(1) Origin A                                   |
  // |Policy: {"default-off": ["self", "Origin B"]}  |
  // +-----------------------------------------------+
  // Features should be enabled by the policy in frame 1 for origins A and B,
  // and disabled for origin C.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultOffFeature, false, {origin_a_, origin_b_}}}});
  EXPECT_TRUE(
      policy1->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_a_));
  EXPECT_TRUE(
      policy1->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_b_));
  EXPECT_FALSE(
      policy1->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_c_));
}

// Test frame policies

TEST_F(FeaturePolicyTest, TestSimpleFramePolicy) {
  // +-------------------------------------------------+
  // |(1)Origin A                                      |
  // |No Policy                                        |
  // |                                                 |
  // |<iframe policy='{"default-self": ["Origin B"]}'> |
  // | +-------------+                                 |
  // | |(2)Origin B  |                                 |
  // | |No Policy    |                                 |
  // | +-------------+                                 |
  // +-------------------------------------------------+
  // Default-self feature should be enabled in cross-origin child frame because
  // permission was delegated through frame policy.
  // This is the same scenario as when the iframe is declared as
  // <iframe allow="default-self">
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedFeaturePolicyHeader frame_policy = {
      {{kDefaultSelfFeature, false, {origin_b_}}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  EXPECT_TRUE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_FALSE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_c_));
  EXPECT_FALSE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_c_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_c_));
}

TEST_F(FeaturePolicyTest, TestAllOriginFramePolicy) {
  // +------------------------------------------+
  // |(1)Origin A                               |
  // |No Policy                                 |
  // |                                          |
  // |<iframe policy='{"default-self": ["*"]}'> |
  // | +-------------+                          |
  // | |(2)Origin B  |                          |
  // | |No Policy    |                          |
  // | +-------------+                          |
  // +------------------------------------------+
  // Default-self feature should be enabled in cross-origin child frame because
  // permission was delegated through frame policy.
  // This is the same scenario that arises when the iframe is declared as
  // <iframe allowfullscreen>
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedFeaturePolicyHeader frame_policy = {
      {{kDefaultSelfFeature, true, std::vector<url::Origin>()}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  EXPECT_TRUE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_FALSE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
  EXPECT_FALSE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_c_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_c_));
}

TEST_F(FeaturePolicyTest, TestFramePolicyCanBeFurtherDelegated) {
  // +-----------------------------------------------------+
  // |(1)Origin A                                          |
  // |No Policy                                            |
  // |                                                     |
  // |<iframe policy='{"default-self": ["Origin B"]}'>     |
  // | +-------------------------------------------------+ |
  // | |(2)Origin B                                      | |
  // | |No Policy                                        | |
  // | |                                                 | |
  // | |<iframe policy='{"default-self": ["Origin C"]}'> | |
  // | | +-------------+                                 | |
  // | | |(3)Origin C  |                                 | |
  // | | |No Policy    |                                 | |
  // | | +-------------+                                 | |
  // | |                                                 | |
  // | |<iframe> (No frame policy)                       | |
  // | | +-------------+                                 | |
  // | | |(4)Origin C  |                                 | |
  // | | |No Policy    |                                 | |
  // | | +-------------+                                 | |
  // | +-------------------------------------------------+ |
  // +-----------------------------------------------------+
  // Default-self feature should be enabled in cross-origin child frames 2 and
  // 3. Feature should be disabled in frame 4 because it was not further
  // delegated through frame policy.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedFeaturePolicyHeader frame_policy1 = {
      {{kDefaultSelfFeature, false, {origin_b_}}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_b_);
  ParsedFeaturePolicyHeader frame_policy2 = {
      {{kDefaultSelfFeature, false, {origin_c_}}}};
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentWithFramePolicy(policy2.get(), frame_policy2, origin_c_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
  EXPECT_TRUE(
      policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_c_));
  EXPECT_FALSE(
      policy4->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_FALSE(
      policy4->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
  EXPECT_FALSE(
      policy4->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_c_));
}

TEST_F(FeaturePolicyTest, TestDefaultOnCanBeDisabledByFramePolicy) {
  // +-------------------------------------+
  // |(1)Origin A                          |
  // |No Policy                            |
  // |                                     |
  // |<iframe policy='{"default-on": []}'> |
  // | +-------------+                     |
  // | |(2)Origin A  |                     |
  // | |No Policy    |                     |
  // | +-------------+                     |
  // |                                     |
  // |<iframe policy='{"default-on": []}'> |
  // | +-------------+                     |
  // | |(3)Origin B  |                     |
  // | |No Policy    |                     |
  // | +-------------+                     |
  // +-------------------------------------+
  // Default-on feature should be disabled in both same-origin and cross-origin
  // child frames because permission was removed through frame policy.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedFeaturePolicyHeader frame_policy1 = {
      {{kDefaultOnFeature, false, std::vector<url::Origin>()}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_a_);
  ParsedFeaturePolicyHeader frame_policy2 = {
      {{kDefaultOnFeature, false, std::vector<url::Origin>()}}};
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy2, origin_b_);
  EXPECT_TRUE(policy1->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_a_));
  EXPECT_TRUE(policy1->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_b_));
  EXPECT_TRUE(policy1->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_c_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_a_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_b_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_c_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_a_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_b_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_c_));
}

TEST_F(FeaturePolicyTest, TestDefaultOffMustBeEnabledByChildFrame) {
  // +------------------------------------------------+
  // |(1)Origin A                                     |
  // |Policy: {"default-off": ["self"]}               |
  // |                                                |
  // |<iframe policy='{"default-off": ["Origin A"]}'> |
  // | +-------------+                                |
  // | |(2)Origin A  |                                |
  // | |No Policy    |                                |
  // | +-------------+                                |
  // |                                                |
  // |<iframe policy='{"default-off": ["Origin B"]}'> |
  // | +-------------+                                |
  // | |(3)Origin B  |                                |
  // | |No Policy    |                                |
  // | +-------------+                                |
  // +------------------------------------------------+
  // Default-off feature should be disabled in both same-origin and cross-origin
  // child frames because they did not declare their own policy to enable it.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy({{{kDefaultOffFeature, false, {origin_a_}}}});
  ParsedFeaturePolicyHeader frame_policy1 = {
      {{kDefaultOffFeature, false, {origin_a_}}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_a_);
  ParsedFeaturePolicyHeader frame_policy2 = {
      {{kDefaultOffFeature, false, {origin_b_}}}};
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy2, origin_b_);
  EXPECT_TRUE(
      policy1->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_a_));
  EXPECT_FALSE(
      policy1->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_b_));
  EXPECT_FALSE(
      policy1->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_c_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_a_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_b_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_c_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_a_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_b_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_c_));
}

TEST_F(FeaturePolicyTest, TestDefaultOffCanBeEnabledByChildFrame) {
  // +------------------------------------------------+
  // |(1)Origin A                                     |
  // |Policy: {"default-off": ["self"]}               |
  // |                                                |
  // |<iframe policy='{"default-off": ["Origin A"]}'> |
  // | +--------------------------------------------+ |
  // | |(2)Origin A                                 | |
  // | |Policy: {"default-off": ["self"]}           | |
  // | +--------------------------------------------+ |
  // |                                                |
  // |<iframe policy='{"default-off": ["Origin B"]}'> |
  // | +--------------------------------------------+ |
  // | |(3)Origin B                                 | |
  // | |Policy: {"default-off": ["self"]}           | |
  // | +--------------------------------------------+ |
  // +------------------------------------------------+
  // Default-off feature should be enabled in both same-origin and cross-origin
  // child frames because it is delegated through the parent's frame policy, and
  // they declare their own policy to enable it.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy({{{kDefaultOffFeature, false, {origin_a_}}}});
  ParsedFeaturePolicyHeader frame_policy1 = {
      {{kDefaultOffFeature, false, {origin_a_}}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_a_);
  policy2->SetHeaderPolicy({{{kDefaultOffFeature, false, {origin_a_}}}});
  ParsedFeaturePolicyHeader frame_policy2 = {
      {{kDefaultOffFeature, false, {origin_b_}}}};
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy2, origin_b_);
  policy3->SetHeaderPolicy({{{kDefaultOffFeature, false, {origin_b_}}}});
  EXPECT_TRUE(
      policy1->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_a_));
  EXPECT_FALSE(
      policy1->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_b_));
  EXPECT_FALSE(
      policy1->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_c_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_a_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_b_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_c_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_a_));
  EXPECT_TRUE(
      policy3->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_b_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultOffFeature, origin_c_));
}

TEST_F(FeaturePolicyTest, TestFramePolicyModifiesHeaderPolicy) {
  // +-----------------------------------------------+
  // |(1)Origin A                                    |
  // |Policy: {"default-self": ["self", "Origin B"]} |
  // |                                               |
  // |<iframe policy='{"default-self": []}'>         |
  // | +-------------------------------------------+ |
  // | |(2)Origin B                                | |
  // | |No Policy                                  | |
  // | +-------------------------------------------+ |
  // |                                               |
  // |<iframe policy='{"default-self": []}'>         |
  // | +-------------------------------------------+ |
  // | |(3)Origin B                                | |
  // | |Policy: {"default-self": ["self"]}         | |
  // | +-------------------------------------------+ |
  // +-----------------------------------------------+
  // Default-self feature should be disabled in both cross-origin child frames
  // by frame policy, even though the parent frame's header policy would
  // otherwise enable it. This is true regardless of the child frame's header
  // policy.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, false, {origin_a_, origin_b_}}}});
  ParsedFeaturePolicyHeader frame_policy1 = {
      {{kDefaultSelfFeature, false, std::vector<url::Origin>()}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_b_);
  ParsedFeaturePolicyHeader frame_policy2 = {
      {{kDefaultSelfFeature, false, std::vector<url::Origin>()}}};
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy2, origin_b_);
  policy3->SetHeaderPolicy({{{kDefaultSelfFeature, false, {origin_b_}}}});
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
}

TEST_F(FeaturePolicyTest, TestCombineFrameAndHeaderPolicies) {
  // +-------------------------------------------------+
  // |(1)Origin A                                      |
  // |No Policy                                        |
  // |                                                 |
  // |<iframe policy='{"default-self": ["Origin B"]}'> |
  // | +---------------------------------------------+ |
  // | |(2)Origin B                                  | |
  // | |Policy: {"default-self": ["*"]}              | |
  // | |                                             | |
  // | |<iframe policy='{"default-self": []}'>       | |
  // | | +-------------+                             | |
  // | | |(3)Origin C  |                             | |
  // | | |No Policy    |                             | |
  // | | +-------------+                             | |
  // | |                                             | |
  // | |<iframe> (No frame policy)                   | |
  // | | +-------------+                             | |
  // | | |(4)Origin C  |                             | |
  // | | |No Policy    |                             | |
  // | | +-------------+                             | |
  // | +---------------------------------------------+ |
  // +-------------------------------------------------+
  // Default-self feature should be enabled in cross-origin child frames 2 and
  // 4. Feature should be disabled in frame 3 by frame policy.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedFeaturePolicyHeader frame_policy1 = {
      {{kDefaultSelfFeature, false, {origin_b_}}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_b_);
  policy2->SetHeaderPolicy(
      {{{kDefaultSelfFeature, true, std::vector<url::Origin>()}}});
  ParsedFeaturePolicyHeader frame_policy2 = {
      {{kDefaultSelfFeature, false, std::vector<url::Origin>()}}};
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentWithFramePolicy(policy2.get(), frame_policy2, origin_c_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_TRUE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_c_));
  EXPECT_TRUE(
      policy4->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_c_));
}

}  // namespace content
