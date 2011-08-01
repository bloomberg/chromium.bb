// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/pref_proxy_config_service.h"

#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/net/ssl_config_service_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/browser/browser_thread.h"
#include "net/base/ssl_config_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ListValue;
using base::Value;
using net::SSLConfig;
using net::SSLConfigService;

class SSLConfigServiceManagerPrefTest : public testing::Test {
 public:
  SSLConfigServiceManagerPrefTest() {}

  virtual void SetUp() {
    message_loop_.reset(new MessageLoop());
    ui_thread_.reset(
        new BrowserThread(BrowserThread::UI, message_loop_.get()));
    io_thread_.reset(
        new BrowserThread(BrowserThread::IO, message_loop_.get()));
    pref_service_.reset(new TestingPrefService());
    SSLConfigServiceManager::RegisterPrefs(pref_service_.get());
  }

  virtual void TearDown() {
    pref_service_.reset();
    io_thread_.reset();
    ui_thread_.reset();
    message_loop_.reset();
  }

 protected:
  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<BrowserThread> ui_thread_;
  scoped_ptr<BrowserThread> io_thread_;
  scoped_ptr<TestingPrefService> pref_service_;
};

// Test that cipher suites can be disabled. "Good" refers to the fact that
// every value is expected to be successfully parsed into a cipher suite.
TEST_F(SSLConfigServiceManagerPrefTest, GoodDisabledCipherSuites) {
  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(pref_service_.get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig old_config;
  config_service->GetSSLConfig(&old_config);
  EXPECT_TRUE(old_config.disabled_cipher_suites.empty());

  ListValue* list_value = new ListValue();
  list_value->Append(Value::CreateStringValue("0x0004"));
  list_value->Append(Value::CreateStringValue("0x0005"));
  pref_service_->SetUserPref(prefs::kCipherSuiteBlacklist, list_value);

  // Pump the message loop to notify the SSLConfigServiceManagerPref that the
  // preferences changed.
  message_loop_->RunAllPending();

  SSLConfig config;
  config_service->GetSSLConfig(&config);

  EXPECT_NE(old_config.disabled_cipher_suites, config.disabled_cipher_suites);
  ASSERT_EQ(2u, config.disabled_cipher_suites.size());
  EXPECT_EQ(0x0004, config.disabled_cipher_suites[0]);
  EXPECT_EQ(0x0005, config.disabled_cipher_suites[1]);
}

// Test that cipher suites can be disabled. "Bad" refers to the fact that
// there are one or more non-cipher suite strings in the preference. They
// should be ignored.
TEST_F(SSLConfigServiceManagerPrefTest, BadDisabledCipherSuites) {
  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(pref_service_.get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig old_config;
  config_service->GetSSLConfig(&old_config);
  EXPECT_TRUE(old_config.disabled_cipher_suites.empty());

  ListValue* list_value = new ListValue();
  list_value->Append(Value::CreateStringValue("0x0004"));
  list_value->Append(Value::CreateStringValue("TLS_NOT_WITH_A_CIPHER_SUITE"));
  list_value->Append(Value::CreateStringValue("0x0005"));
  list_value->Append(Value::CreateStringValue("0xBEEFY"));
  pref_service_->SetUserPref(prefs::kCipherSuiteBlacklist, list_value);

  // Pump the message loop to notify the SSLConfigServiceManagerPref that the
  // preferences changed.
  message_loop_->RunAllPending();

  SSLConfig config;
  config_service->GetSSLConfig(&config);

  EXPECT_NE(old_config.disabled_cipher_suites, config.disabled_cipher_suites);
  ASSERT_EQ(2u, config.disabled_cipher_suites.size());
  EXPECT_EQ(0x0004, config.disabled_cipher_suites[0]);
  EXPECT_EQ(0x0005, config.disabled_cipher_suites[1]);
}
