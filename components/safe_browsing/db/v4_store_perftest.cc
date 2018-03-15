// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_simple_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/db/v4_test_util.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace safe_browsing {

class V4StorePerftest : public testing::Test {};

TEST_F(V4StorePerftest, StressTest) {
  const int kNumPrefixes = 2000000;
  std::vector<std::string> prefixes;
  std::vector<std::string> full_hashes;
  for (size_t i = 0; i < kNumPrefixes; i++) {
    std::string sha256 = crypto::SHA256HashString(base::StringPrintf("%zu", i));
    DCHECK_EQ(crypto::kSHA256Length, kMaxHashPrefixLength);
    full_hashes.push_back(sha256);
    prefixes.push_back(sha256.substr(0, kMinHashPrefixLength));
  }

  auto store = std::make_unique<TestV4Store>(
      base::MakeRefCounted<base::TestSimpleTaskRunner>(), base::FilePath());
  store->SetPrefixes(std::move(prefixes), kMinHashPrefixLength);

  size_t matches = 0;
  base::ElapsedTimer timer;
  for (const auto& full_hash : full_hashes) {
    matches += !store->GetMatchingHashPrefix(full_hash).empty();
  }
  perf_test::PrintResult("GetMachingHashPrefix", "", "",
                         timer.Elapsed().InMillisecondsF(), "ms", true);

  EXPECT_EQ(matches, full_hashes.size());
}

}  // namespace safe_browsing
