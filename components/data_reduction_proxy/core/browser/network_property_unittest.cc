// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/network_property.h"

#include <string>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

// Test that the network property is properly serialized and unserialized back.
TEST(NetworkPropertyTest, TestSerializeUnserialize) {
  for (int secure_proxy_state = 0; secure_proxy_state <= PROXY_STATE_LAST;
       ++secure_proxy_state) {
    for (int insecure_proxy_state = 0; insecure_proxy_state <= PROXY_STATE_LAST;
         ++insecure_proxy_state) {
      NetworkProperty network_property(
          static_cast<ProxyState>(secure_proxy_state),
          static_cast<ProxyState>(insecure_proxy_state));

      EXPECT_EQ(static_cast<ProxyState>(secure_proxy_state),
                network_property.secure_proxies_state());
      EXPECT_EQ(static_cast<ProxyState>(insecure_proxy_state),
                network_property.insecure_proxies_state());

      std::string network_property_string = network_property.ToString();
      EXPECT_FALSE(network_property_string.empty());

      // Generate network property from the string and compare it with
      // |network_property|.
      NetworkProperty network_property_reversed(network_property_string);
      EXPECT_EQ(network_property.secure_proxies_state(),
                network_property_reversed.secure_proxies_state());
      EXPECT_EQ(network_property.insecure_proxies_state(),
                network_property_reversed.insecure_proxies_state());
      EXPECT_EQ(network_property.ToString(),
                network_property_reversed.ToString());
    }
  }
}

}  // namespace

}  // namespace data_reduction_proxy