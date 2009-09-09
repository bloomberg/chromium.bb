// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_config_service_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;

namespace {

class SSLConfigServiceMacTest : public testing::Test {
};

}  // namespace

TEST(SSLConfigServiceMacTest, GetNowTest) {
  // Verify that the constructor sets the correct default values.
  net::SSLConfig config;
  EXPECT_EQ(true, config.rev_checking_enabled);
  EXPECT_EQ(false, config.ssl2_enabled);
  EXPECT_EQ(true, config.ssl3_enabled);
  EXPECT_EQ(true, config.tls1_enabled);

  bool rv = net::SSLConfigServiceMac::GetSSLConfigNow(&config);
  EXPECT_TRUE(rv);
}

TEST(SSLConfigServiceMacTest, SetTest) {
  // Save the current settings so we can restore them after the tests.
  net::SSLConfig config_save;
  bool rv = net::SSLConfigServiceMac::GetSSLConfigNow(&config_save);
  EXPECT_TRUE(rv);

  net::SSLConfig config;

  // Test SetRevCheckingEnabled.
  net::SSLConfigServiceMac::SetRevCheckingEnabled(true);
  rv = net::SSLConfigServiceMac::GetSSLConfigNow(&config);
  EXPECT_TRUE(rv);
  EXPECT_TRUE(config.rev_checking_enabled);

  net::SSLConfigServiceMac::SetRevCheckingEnabled(false);
  rv = net::SSLConfigServiceMac::GetSSLConfigNow(&config);
  EXPECT_TRUE(rv);
  EXPECT_FALSE(config.rev_checking_enabled);

  net::SSLConfigServiceMac::SetRevCheckingEnabled(
      config_save.rev_checking_enabled);

  // Test SetSSL2Enabled.
  net::SSLConfigServiceMac::SetSSL2Enabled(true);
  rv = net::SSLConfigServiceMac::GetSSLConfigNow(&config);
  EXPECT_TRUE(rv);
  EXPECT_TRUE(config.ssl2_enabled);

  net::SSLConfigServiceMac::SetSSL2Enabled(false);
  rv = net::SSLConfigServiceMac::GetSSLConfigNow(&config);
  EXPECT_TRUE(rv);
  EXPECT_FALSE(config.ssl2_enabled);

  net::SSLConfigServiceMac::SetSSL2Enabled(config_save.ssl2_enabled);

  // Test SetSSL3Enabled.
  net::SSLConfigServiceMac::SetSSL3Enabled(true);
  rv = net::SSLConfigServiceMac::GetSSLConfigNow(&config);
  EXPECT_TRUE(rv);
  EXPECT_TRUE(config.ssl3_enabled);

  net::SSLConfigServiceMac::SetSSL3Enabled(false);
  rv = net::SSLConfigServiceMac::GetSSLConfigNow(&config);
  EXPECT_TRUE(rv);
  EXPECT_FALSE(config.ssl3_enabled);

  net::SSLConfigServiceMac::SetSSL3Enabled(config_save.ssl3_enabled);

  // Test SetTLS1Enabled.
  net::SSLConfigServiceMac::SetTLS1Enabled(true);
  rv = net::SSLConfigServiceMac::GetSSLConfigNow(&config);
  EXPECT_TRUE(rv);
  EXPECT_TRUE(config.tls1_enabled);

  net::SSLConfigServiceMac::SetTLS1Enabled(false);
  rv = net::SSLConfigServiceMac::GetSSLConfigNow(&config);
  EXPECT_TRUE(rv);
  EXPECT_FALSE(config.tls1_enabled);

  net::SSLConfigServiceMac::SetTLS1Enabled(config_save.tls1_enabled);
}

TEST(SSLConfigServiceMacTest, GetTest) {
  TimeTicks now = TimeTicks::Now();
  TimeTicks now_1 = now + TimeDelta::FromSeconds(1);
  TimeTicks now_11 = now + TimeDelta::FromSeconds(11);

  net::SSLConfig config, config_1, config_11;
  scoped_refptr<net::SSLConfigServiceMac> config_service(
      new net::SSLConfigServiceMac(now));
  config_service->GetSSLConfigAt(&config, now);

  // Flip rev_checking_enabled.
  net::SSLConfigServiceMac::SetRevCheckingEnabled(
      !config.rev_checking_enabled);

  config_service->GetSSLConfigAt(&config_1, now_1);
  EXPECT_EQ(config.rev_checking_enabled, config_1.rev_checking_enabled);

  config_service->GetSSLConfigAt(&config_11, now_11);
  EXPECT_EQ(!config.rev_checking_enabled, config_11.rev_checking_enabled);

  // Restore the original value.
  net::SSLConfigServiceMac::SetRevCheckingEnabled(
      config.rev_checking_enabled);
}
