// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_feature_list_creator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class AwContentBrowserClientTest : public testing::Test {};

TEST_F(AwContentBrowserClientTest, DisableCreatingTaskScheduler) {
  AwFeatureListCreator aw_feature_list_creator;
  AwContentBrowserClient client(&aw_feature_list_creator);
  EXPECT_TRUE(client.ShouldCreateTaskScheduler());

  AwContentBrowserClient::DisableCreatingTaskScheduler();
  EXPECT_FALSE(client.ShouldCreateTaskScheduler());
}

// Tests that constraints on trust for Symantec-issued certificates are not
// enforced for the NetworkContext, as it should behave like the Android system.
TEST_F(AwContentBrowserClientTest, SymantecPoliciesExempted) {
  AwFeatureListCreator aw_feature_list_creator;
  AwContentBrowserClient client(&aw_feature_list_creator);
  network::mojom::NetworkContextParamsPtr network_context_params =
      client.GetNetworkContextParams();

  ASSERT_TRUE(network_context_params);
  ASSERT_TRUE(network_context_params->initial_ssl_config);
  ASSERT_TRUE(network_context_params->initial_ssl_config
                  ->symantec_enforcement_disabled);
}

// Tests that SHA-1 is still allowed for locally-installed trust anchors,
// including those in application manifests, as it should behave like
// the Android system.
TEST_F(AwContentBrowserClientTest, SHA1LocalAnchorsAllowed) {
  AwFeatureListCreator aw_feature_list_creator;
  AwContentBrowserClient client(&aw_feature_list_creator);
  network::mojom::NetworkContextParamsPtr network_context_params =
      client.GetNetworkContextParams();

  ASSERT_TRUE(network_context_params);
  ASSERT_TRUE(network_context_params->initial_ssl_config);
  ASSERT_TRUE(
      network_context_params->initial_ssl_config->sha1_local_anchors_enabled);
}

}  // namespace android_webview
