// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_service.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/net/dns_probe_runner.h"
#include "chrome/browser/net/dns_probe_test_util.h"
#include "chrome/common/net/net_error_info.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/dns/dns_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MessageLoopForIO;
using base::RunLoop;
using chrome_common_net::DnsProbeStatus;
using content::TestBrowserThreadBundle;
using net::MockDnsClientRule;

namespace chrome_browser_net {

namespace {

class DnsProbeServiceTest : public testing::Test {
 public:
  DnsProbeServiceTest()
      : callback_called_(false),
        callback_result_(chrome_common_net::DNS_PROBE_MAX) {
  }

  void Probe() {
    service_.ProbeDns(base::Bind(&DnsProbeServiceTest::ProbeCallback,
                                 base::Unretained(this)));
  }

  void Reset() {
    callback_called_ = false;
  }

 protected:
  void SetRules(MockDnsClientRule::Result system_query_result,
                MockDnsClientRule::Result public_query_result) {
    service_.SetSystemClientForTesting(
        CreateMockDnsClientForProbes(system_query_result));
    service_.SetPublicClientForTesting(
        CreateMockDnsClientForProbes(public_query_result));
  }

  void RunTest(MockDnsClientRule::Result system_query_result,
               MockDnsClientRule::Result public_query_result,
               DnsProbeStatus expected_result) {
    Reset();
    SetRules(system_query_result, public_query_result);

    Probe();
    RunLoop().RunUntilIdle();
    EXPECT_TRUE(callback_called_);
    EXPECT_EQ(expected_result, callback_result_);
  }

  void ClearCachedResult() {
    service_.ClearCachedResultForTesting();
  }

 private:
  void ProbeCallback(DnsProbeStatus result) {
    EXPECT_FALSE(callback_called_);
    callback_called_ = true;
    callback_result_ = result;
  }

  DnsProbeService service_;
  bool callback_called_;
  DnsProbeStatus callback_result_;
  TestBrowserThreadBundle bundle_;
};

TEST_F(DnsProbeServiceTest, Probe_OK_OK) {
  RunTest(MockDnsClientRule::OK, MockDnsClientRule::OK,
          chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
}

TEST_F(DnsProbeServiceTest, Probe_TIMEOUT_OK) {
  RunTest(MockDnsClientRule::TIMEOUT, MockDnsClientRule::OK,
          chrome_common_net::DNS_PROBE_FINISHED_BAD_CONFIG);
}

TEST_F(DnsProbeServiceTest, Probe_TIMEOUT_TIMEOUT) {
  RunTest(MockDnsClientRule::TIMEOUT, MockDnsClientRule::TIMEOUT,
          chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET);
}

TEST_F(DnsProbeServiceTest, Probe_OK_FAIL) {
  RunTest(MockDnsClientRule::OK, MockDnsClientRule::FAIL,
          chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
}

TEST_F(DnsProbeServiceTest, Probe_FAIL_OK) {
  RunTest(MockDnsClientRule::FAIL, MockDnsClientRule::OK,
          chrome_common_net::DNS_PROBE_FINISHED_BAD_CONFIG);
}

TEST_F(DnsProbeServiceTest, Probe_FAIL_FAIL) {
  RunTest(MockDnsClientRule::FAIL, MockDnsClientRule::FAIL,
          chrome_common_net::DNS_PROBE_FINISHED_INCONCLUSIVE);
}

TEST_F(DnsProbeServiceTest, Cache) {
  RunTest(MockDnsClientRule::OK, MockDnsClientRule::OK,
          chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  // Cached NXDOMAIN result should persist, not the result from the new rules.
  RunTest(MockDnsClientRule::TIMEOUT, MockDnsClientRule::TIMEOUT,
          chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
}

TEST_F(DnsProbeServiceTest, Expire) {
  RunTest(MockDnsClientRule::OK, MockDnsClientRule::OK,
          chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  // Pretend cache expires.
  ClearCachedResult();
  // New rules should apply, since a new probe should be run.
  RunTest(MockDnsClientRule::TIMEOUT, MockDnsClientRule::TIMEOUT,
          chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET);
}

}  // namespace

}  // namespace chrome_browser_net
