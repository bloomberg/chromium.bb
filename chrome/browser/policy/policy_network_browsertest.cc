// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/policy_constants.h"

namespace {

void VerifyQuicEnabledStatus(net::URLRequestContextGetter* getter,
                             bool quic_enabled_expected,
                             const base::Closure& done_callback) {
  net::URLRequestContext* context = getter->GetURLRequestContext();
  bool quic_enabled = context->http_transaction_factory()->GetSession()->
        params().enable_quic;
  EXPECT_EQ(quic_enabled_expected, quic_enabled);

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   done_callback);
}

void VerifyQuicEnabledStatusInIOThread(bool quic_enabled_expected) {
  base::RunLoop run_loop;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(VerifyQuicEnabledStatus,
                 base::RetainedRef(g_browser_process->system_request_context()),
                 quic_enabled_expected, run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace

namespace policy {

// The tests are based on the assumption that command line flag kEnableQuic
// guarantees that QUIC protocol is enabled which is the case at the moment
// when these are being written.
class QuicAllowedPolicyTestBase: public InProcessBrowserTest {
 public:
  QuicAllowedPolicyTestBase() : InProcessBrowserTest(){}

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableQuic);
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));

    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    PolicyMap values;
    GetQuicAllowedPolicy(&values);
    provider_.UpdateChromePolicy(values);
  }

  virtual void GetQuicAllowedPolicy(PolicyMap* values) = 0;

 private:
  MockConfigurationPolicyProvider provider_;
  DISALLOW_COPY_AND_ASSIGN(QuicAllowedPolicyTestBase);
};

// Policy QuicAllowed set to false.
class QuicAllowedPolicyIsFalse: public QuicAllowedPolicyTestBase {
 public:
  QuicAllowedPolicyIsFalse() : QuicAllowedPolicyTestBase(){}

 protected:
  void GetQuicAllowedPolicy(PolicyMap* values) override {
    values->Set(key::kQuicAllowed, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                POLICY_SOURCE_CLOUD,
                base::WrapUnique(new base::FundamentalValue(false)), nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicAllowedPolicyIsFalse);
};

IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsFalse, QuicDisallowed) {
  VerifyQuicEnabledStatusInIOThread(false);
}

// Policy QuicAllowed set to true.
class QuicAllowedPolicyIsTrue: public QuicAllowedPolicyTestBase {
 public:
  QuicAllowedPolicyIsTrue() : QuicAllowedPolicyTestBase(){}

 protected:
  void GetQuicAllowedPolicy(PolicyMap* values) override {
    values->Set(key::kQuicAllowed, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                POLICY_SOURCE_CLOUD,
                base::WrapUnique(new base::FundamentalValue(true)), nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicAllowedPolicyIsTrue);
};

IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsTrue, QuicAllowed) {
  VerifyQuicEnabledStatusInIOThread(true);
}

// Policy QuicAllowed is not set.
class QuicAllowedPolicyIsNotSet: public QuicAllowedPolicyTestBase {
 public:
  QuicAllowedPolicyIsNotSet() : QuicAllowedPolicyTestBase(){}

 protected:
  void GetQuicAllowedPolicy(PolicyMap* values) override {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicAllowedPolicyIsNotSet);
};

IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsNotSet, NoQuicRegulations) {
  VerifyQuicEnabledStatusInIOThread(true);
}

}  // namespace policy
