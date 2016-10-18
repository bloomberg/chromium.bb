// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_TIME_NETWORK_TIME_TEST_UTILS_H_
#define COMPONENTS_NETWORK_TIME_NETWORK_TIME_TEST_UTILS_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace test {
class ScopedFeatureList;
};  // namespace test

class FieldTrialList;
}  // namespace base

namespace network_time {
// A test fixture that allows configuring the network time queries field trial.
class FieldTrialTest : public ::testing::Test {
 public:
  FieldTrialTest();
  ~FieldTrialTest() override;

 protected:
  void SetNetworkQueriesWithVariationsService(bool enable,
                                              float query_probability);

 private:
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrialTest);
};

}  // namespace network_time

#endif  // COMPONENTS_NETWORK_TIME_NETWORK_TIME_TEST_UTILS_H_
