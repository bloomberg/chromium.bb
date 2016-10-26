// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_TIME_NETWORK_TIME_TEST_UTILS_H_
#define COMPONENTS_NETWORK_TIME_NETWORK_TIME_TEST_UTILS_H_

#include <map>
#include <memory>

#include "base/macros.h"

namespace base {
namespace test {
class ScopedFeatureList;
};  // namespace test

class FieldTrialList;
}  // namespace base

namespace net {
namespace test_server {
struct HttpRequest;
class HttpResponse;
}  // namespace test_server
}  // namespace net

namespace network_time {

std::unique_ptr<net::test_server::HttpResponse> GoodTimeResponseHandler(
    const net::test_server::HttpRequest& request);

// Allows tests to configure the network time queries field trial.
class FieldTrialTest {
 public:
  enum FetchesOnDemandStatus {
    ENABLE_FETCHES_ON_DEMAND,
    DISABLE_FETCHES_ON_DEMAND,
  };

  FieldTrialTest();
  virtual ~FieldTrialTest();
  void SetNetworkQueriesWithVariationsService(
      bool enable,
      float query_probability,
      FetchesOnDemandStatus fetches_on_demand);

 private:
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrialTest);
};

}  // namespace network_time

#endif  // COMPONENTS_NETWORK_TIME_NETWORK_TIME_TEST_UTILS_H_
