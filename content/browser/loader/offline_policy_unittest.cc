// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/offline_policy.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/resource_type.h"

namespace content {

class OfflinePolicyTest : public testing::Test {
 protected:
  virtual void SetUp() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableOfflineCacheAccess);
    policy_ = new OfflinePolicy;
  }

  virtual void TearDown() {
    delete policy_;
    policy_ = NULL;
  }

  OfflinePolicy* policy_;
};

// Confirm that the initial state of an offline object is to return
// LOAD_FROM_CACHE_IF_OFFLINE until it gets changed.
TEST_F(OfflinePolicyTest, InitialState) {
  // Two loads without any reset, no UpdateStateForSuccessfullyStartedRequest.
  EXPECT_EQ(net::LOAD_FROM_CACHE_IF_OFFLINE,
            policy_->GetAdditionalLoadFlags(0, true));
  EXPECT_EQ(net::LOAD_FROM_CACHE_IF_OFFLINE,
            policy_->GetAdditionalLoadFlags(0, false));
}

// Completion without any network probing doesn't change result value.
TEST_F(OfflinePolicyTest, CompletedUncertain) {
  EXPECT_EQ(net::LOAD_FROM_CACHE_IF_OFFLINE,
            policy_->GetAdditionalLoadFlags(0, true));
  net::HttpResponseInfo response_info;
  policy_->UpdateStateForSuccessfullyStartedRequest(response_info);
  EXPECT_EQ(net::LOAD_FROM_CACHE_IF_OFFLINE,
            policy_->GetAdditionalLoadFlags(0, false));
}

// Completion with a failed network probe changes result value.
TEST_F(OfflinePolicyTest, CompletedNoNetwork) {
  EXPECT_EQ(net::LOAD_FROM_CACHE_IF_OFFLINE,
            policy_->GetAdditionalLoadFlags(0, true));
  net::HttpResponseInfo response_info;
  response_info.server_data_unavailable = true;
  policy_->UpdateStateForSuccessfullyStartedRequest(response_info);
  EXPECT_EQ(net::LOAD_ONLY_FROM_CACHE,
            policy_->GetAdditionalLoadFlags(0, false));
}

// Completion with a successful network probe changes result value.
TEST_F(OfflinePolicyTest, CompletedNetwork) {
  EXPECT_EQ(net::LOAD_FROM_CACHE_IF_OFFLINE,
            policy_->GetAdditionalLoadFlags(0, true));
  net::HttpResponseInfo response_info;
  response_info.network_accessed = true;
  policy_->UpdateStateForSuccessfullyStartedRequest(response_info);
  EXPECT_EQ(0, policy_->GetAdditionalLoadFlags(0, false));
}

// A new navigation resets a state change.
TEST_F(OfflinePolicyTest, NewNavigationReset) {
  EXPECT_EQ(net::LOAD_FROM_CACHE_IF_OFFLINE,
            policy_->GetAdditionalLoadFlags(0, true));
  net::HttpResponseInfo response_info;
  response_info.network_accessed = true;
  policy_->UpdateStateForSuccessfullyStartedRequest(response_info);
  EXPECT_EQ(0, policy_->GetAdditionalLoadFlags(0, false));
  EXPECT_EQ(net::LOAD_FROM_CACHE_IF_OFFLINE,
            policy_->GetAdditionalLoadFlags(0, true));
  EXPECT_EQ(net::LOAD_FROM_CACHE_IF_OFFLINE,
            policy_->GetAdditionalLoadFlags(0, false));
}

// Cache related flags inhibit the returning of any special flags.
TEST_F(OfflinePolicyTest, ConsumerFlagOverride) {
  EXPECT_EQ(0, policy_->GetAdditionalLoadFlags(net::LOAD_BYPASS_CACHE, true));
  net::HttpResponseInfo response_info;
  response_info.server_data_unavailable = true;
  policy_->UpdateStateForSuccessfullyStartedRequest(response_info);
  EXPECT_EQ(0, policy_->GetAdditionalLoadFlags(net::LOAD_BYPASS_CACHE, false));
}

}
