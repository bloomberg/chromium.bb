// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_job.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "net/base/net_log.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::DnsClient;
using net::DnsConfig;
using net::IPAddressNumber;
using net::IPEndPoint;
using net::ParseIPLiteralToNumber;
using net::MockDnsClientRule;
using net::MockDnsClientRuleList;
using net::NetLog;

namespace chrome_browser_net {

namespace {

class DnsProbeJobTest : public testing::Test {
 public:
  void RunProbe(MockDnsClientRule::Result expected_good_result,
                MockDnsClientRule::Result expected_bad_result);

 protected:
  void OnProbeFinished(DnsProbeJob* job, DnsProbeJob::Result result);

  bool callback_called_;
  DnsProbeJob::Result callback_result_;
};

// Runs a probe and waits for the callback.  |good_result| and |bad_result|
// are the result of the good and bad transactions that the DnsProbeJob will
// receive.
void DnsProbeJobTest::RunProbe(MockDnsClientRule::Result good_result,
                               MockDnsClientRule::Result bad_result) {
  DnsConfig config;
  config.nameservers.clear();
  IPAddressNumber dns_ip;
  ParseIPLiteralToNumber("192.168.1.1", &dns_ip);
  const uint16 kDnsPort = net::dns_protocol::kDefaultPort;
  config.nameservers.push_back(IPEndPoint(dns_ip, kDnsPort));

  const uint16 kTypeA = net::dns_protocol::kTypeA;
  MockDnsClientRuleList rules;
  rules.push_back(MockDnsClientRule("google.com", kTypeA, good_result));
  rules.push_back(MockDnsClientRule("", kTypeA, bad_result));
  scoped_ptr<DnsClient> dns_client = CreateMockDnsClient(config, rules);
  dns_client->SetConfig(config);

  NetLog* net_log = NULL;
  DnsProbeJob::CallbackType callback = base::Bind(
      &DnsProbeJobTest::OnProbeFinished,
      base::Unretained(this));

  // Need to set these before creating job, because it can call the callback
  // synchronously in the constructor if both transactions fail to start.
  callback_called_ = false;
  callback_result_ = DnsProbeJob::SERVERS_UNKNOWN;

  // DnsProbeJob needs somewhere to post the callback.
  scoped_ptr<MessageLoop> message_loop_(new MessageLoopForIO());

  scoped_ptr<DnsProbeJob> job(
      DnsProbeJob::CreateJob(dns_client.Pass(), callback, net_log));

  // Force callback to run.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

void DnsProbeJobTest::OnProbeFinished(DnsProbeJob* job,
                                      DnsProbeJob::Result result) {
  EXPECT_FALSE(callback_called_);

  callback_called_ = true;
  callback_result_ = result;
}

struct TestCase {
  MockDnsClientRule::Result good_result;
  MockDnsClientRule::Result bad_result;
  DnsProbeJob::Result expected_probe_result;
};

TEST_F(DnsProbeJobTest, Test) {
  static const TestCase kTestCases[] = {
    { MockDnsClientRule::OK,
      MockDnsClientRule::EMPTY,
      DnsProbeJob::SERVERS_CORRECT },
    { MockDnsClientRule::EMPTY,
      MockDnsClientRule::EMPTY,
      DnsProbeJob::SERVERS_INCORRECT },
    // TODO(ttuttle): Test that triggers QUERY_DNS_ERROR.
    //                (Need to add another mock behavior to MockDnsClient.)
    { MockDnsClientRule::FAIL_ASYNC,
      MockDnsClientRule::FAIL_ASYNC,
      DnsProbeJob::SERVERS_FAILING },
    { MockDnsClientRule::FAIL_SYNC,
      MockDnsClientRule::FAIL_SYNC,
      DnsProbeJob::SERVERS_FAILING },
    { MockDnsClientRule::TIMEOUT,
      MockDnsClientRule::TIMEOUT,
      DnsProbeJob::SERVERS_UNREACHABLE },
  };

  for (size_t i = 0; i < arraysize(kTestCases); i++) {
    const TestCase* test_case = &kTestCases[i];
    RunProbe(test_case->good_result, test_case->bad_result);
    EXPECT_TRUE(callback_called_);
    EXPECT_EQ(test_case->expected_probe_result, callback_result_);
  }
}

}  // namespace

}  // namespace chrome_browser_net
