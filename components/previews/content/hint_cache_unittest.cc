// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/hint_cache.h"

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/previews/core/previews_experiments.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {

class HintCacheTest : public testing::Test {
 public:
  HintCacheTest() {}

  ~HintCacheTest() override {}

  DISALLOW_COPY_AND_ASSIGN(HintCacheTest);
};

TEST_F(HintCacheTest, TestMemoryCache) {
  HintCache hint_cache;

  optimization_guide::proto::Hint hint1;
  hint1.set_key("subdomain.domain.org");
  hint1.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Hint hint2;
  hint2.set_key("host.domain.org");
  hint2.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Hint hint3;
  hint3.set_key("otherhost.subdomain.domain.org");
  hint3.set_key_representation(optimization_guide::proto::HOST_SUFFIX);

  hint_cache.AddHint(hint1);
  hint_cache.AddHint(hint2);
  hint_cache.AddHint(hint3);

  // Not matched
  EXPECT_FALSE(hint_cache.HasHint("domain.org"));
  EXPECT_FALSE(hint_cache.HasHint("othersubdomain.domain.org"));

  // Matched
  EXPECT_TRUE(hint_cache.HasHint("otherhost.subdomain.domain.org"));
  EXPECT_TRUE(hint_cache.HasHint("host.subdomain.domain.org"));
  EXPECT_TRUE(hint_cache.HasHint("subhost.host.subdomain.domain.org"));

  // Cached in memory
  EXPECT_TRUE(hint_cache.IsHintLoaded("host.domain.org"));
  EXPECT_TRUE(hint_cache.IsHintLoaded("otherhost.subdomain.domain.org"));
  EXPECT_TRUE(hint_cache.IsHintLoaded("host.subdomain.domain.org"));
  EXPECT_TRUE(hint_cache.IsHintLoaded("subhost.host.subdomain.domain.org"));

  // Matched key
  EXPECT_EQ(hint2.key(), hint_cache.GetHint("host.domain.org")->key());
  EXPECT_EQ(hint3.key(),
            hint_cache.GetHint("otherhost.subdomain.domain.org")->key());
  EXPECT_EQ(hint1.key(),
            hint_cache.GetHint("host.subdomain.domain.org")->key());
}

}  // namespace

}  // namespace previews
