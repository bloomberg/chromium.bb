// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_config_service_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;

namespace {

// SSLClientConfig service caches settings for 10 seconds for performance.
// So we use synthetic time values along with the 'GetSSLConfigAt' method
// to ensure that the current settings are re-read.  By incrementing the time
// value by 11 seconds, we ensure fresh config settings.
const int kSSLConfigNextTimeInternal = 11;

class SSLConfigServiceMacObserver : public net::SSLConfigService::Observer {
 public:
  SSLConfigServiceMacObserver() : change_was_observed_(false) {
  }
  bool change_was_observed() const {
    return change_was_observed_;
  }
 protected:
  virtual void OnSSLConfigChanged() {
    change_was_observed_ = true;
  }
  bool change_was_observed_;
};

class SSLConfigServiceMacTest : public testing::Test {
};

}  // namespace

TEST(SSLConfigServiceMacTest, GetNowTest) {
  // Verify that the constructor sets the correct default values.
  net::SSLConfig config;
  EXPECT_TRUE(config.rev_checking_enabled);
  EXPECT_TRUE(config.ssl3_enabled);
  EXPECT_TRUE(config.tls1_enabled);

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
  TimeTicks later = now + TimeDelta::FromSeconds(kSSLConfigNextTimeInternal);

  net::SSLConfig config, config_1, config_later;
  scoped_refptr<net::SSLConfigServiceMac> config_service(
      new net::SSLConfigServiceMac(now));
  config_service->GetSSLConfigAt(&config, now);

  // Flip rev_checking_enabled.
  net::SSLConfigServiceMac::SetRevCheckingEnabled(
      !config.rev_checking_enabled);

  config_service->GetSSLConfigAt(&config_1, now_1);
  EXPECT_EQ(config.rev_checking_enabled, config_1.rev_checking_enabled);

  config_service->GetSSLConfigAt(&config_later, later);
  EXPECT_EQ(!config.rev_checking_enabled, config_later.rev_checking_enabled);

  // Restore the original value.
  net::SSLConfigServiceMac::SetRevCheckingEnabled(
      config.rev_checking_enabled);
}

TEST(SSLConfigServiceMacTest, ObserverTest) {
  TimeTicks now = TimeTicks::Now();
  TimeTicks later = now + TimeDelta::FromSeconds(kSSLConfigNextTimeInternal);

  scoped_refptr<net::SSLConfigServiceMac> config_service(
      new net::SSLConfigServiceMac(now));

  // Save the current settings so we can restore them after the tests.
  net::SSLConfig config_save;
  bool rv = net::SSLConfigServiceMac::GetSSLConfigNow(&config_save);
  EXPECT_TRUE(rv);

  net::SSLConfig config;

  // Add an observer.
  SSLConfigServiceMacObserver observer;
  config_service->AddObserver(&observer);

  // Toggle SSL3.
  net::SSLConfigServiceMac::SetSSL3Enabled(!config_save.ssl3_enabled);
  config_service->GetSSLConfigAt(&config, later);

  // Verify that the observer was notified.
  EXPECT_TRUE(observer.change_was_observed());

  // Remove the observer.
  config_service->RemoveObserver(&observer);

  // Restore the original SSL3 setting.
  net::SSLConfigServiceMac::SetSSL3Enabled(config_save.ssl3_enabled);
}

