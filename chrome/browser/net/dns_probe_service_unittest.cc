// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_service.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/net/dns_probe_job.h"
#include "chrome/common/net/net_error_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using chrome_common_net::DnsProbeResult;

namespace chrome_browser_net {

namespace {

class MockDnsProbeJob : public DnsProbeJob {
 public:
  MockDnsProbeJob(const CallbackType& callback,
                  DnsProbeJob::Result result)
      : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MockDnsProbeJob::CallCallback,
                   weak_factory_.GetWeakPtr(),
                   callback,
                   result));
  }

  virtual ~MockDnsProbeJob() { }

 private:
  void CallCallback(const CallbackType& callback, Result result) {
    callback.Run(this, result);
  }

  base::WeakPtrFactory<MockDnsProbeJob> weak_factory_;
};

class TestDnsProbeService : public DnsProbeService {
 public:
  TestDnsProbeService()
      : DnsProbeService(),
        system_job_created_(false),
        public_job_created_(false),
        mock_system_result_(DnsProbeJob::SERVERS_UNKNOWN),
        mock_public_result_(DnsProbeJob::SERVERS_UNKNOWN),
        mock_system_fail_(false) {
  }

  virtual ~TestDnsProbeService() { }

  void set_mock_results(
      DnsProbeJob::Result mock_system_result,
      DnsProbeJob::Result mock_public_result) {
    mock_system_result_ = mock_system_result;
    mock_public_result_ = mock_public_result;
  }

  void set_mock_system_fail(bool mock_system_fail) {
    mock_system_fail_ = mock_system_fail;
  }

  bool jobs_created(void) {
    return system_job_created_ && public_job_created_;
  }

  void ResetJobsCreated() {
    system_job_created_ = false;
    public_job_created_ = false;
  }

  void MockExpireResults() {
    ExpireResults();
  }

  bool system_job_created_;
  bool public_job_created_;

 private:
  // Override methods in DnsProbeService to return mock jobs:

  virtual scoped_ptr<DnsProbeJob> CreateSystemProbeJob(
      const DnsProbeJob::CallbackType& job_callback) OVERRIDE {
    if (mock_system_fail_)
      return scoped_ptr<DnsProbeJob>(NULL);

    system_job_created_ = true;
    return scoped_ptr<DnsProbeJob>(
        new MockDnsProbeJob(job_callback,
                            mock_system_result_));
  }

  virtual scoped_ptr<DnsProbeJob> CreatePublicProbeJob(
      const DnsProbeJob::CallbackType& job_callback) OVERRIDE {
    public_job_created_ = true;
    return scoped_ptr<DnsProbeJob>(
        new MockDnsProbeJob(job_callback,
                            mock_public_result_));
  }

  DnsProbeJob::Result mock_system_result_;
  DnsProbeJob::Result mock_public_result_;
  bool mock_system_fail_;
};

class DnsProbeServiceTest : public testing::Test {
 public:
  DnsProbeServiceTest()
      : callback_called_(false),
        callback_result_(chrome_common_net::DNS_PROBE_UNKNOWN) {
  }

  void Probe() {
    service_.ProbeDns(base::Bind(&DnsProbeServiceTest::ProbeCallback,
                                 base::Unretained(this)));
  }

  void RunUntilIdle() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  void Reset() {
    service_.ResetJobsCreated();
    callback_called_ = false;
  }

  MessageLoopForIO message_loop_;
  TestDnsProbeService service_;
  bool callback_called_;
  DnsProbeResult callback_result_;

 private:
  void ProbeCallback(DnsProbeResult result) {
    callback_called_ = true;
    callback_result_ = result;
  }
};

TEST_F(DnsProbeServiceTest, Null) {
}

TEST_F(DnsProbeServiceTest, Probe) {
  service_.set_mock_results(DnsProbeJob::SERVERS_CORRECT,
                            DnsProbeJob::SERVERS_CORRECT);

  Probe();
  EXPECT_TRUE(service_.jobs_created());
  EXPECT_FALSE(callback_called_);

  RunUntilIdle();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(chrome_common_net::DNS_PROBE_NXDOMAIN, callback_result_);
}

TEST_F(DnsProbeServiceTest, Cache) {
  service_.set_mock_results(DnsProbeJob::SERVERS_CORRECT,
                            DnsProbeJob::SERVERS_CORRECT);

  Probe();
  RunUntilIdle();
  Reset();

  // Cached NXDOMAIN result should persist.

  Probe();
  EXPECT_FALSE(service_.jobs_created());

  RunUntilIdle();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(chrome_common_net::DNS_PROBE_NXDOMAIN, callback_result_);
}

TEST_F(DnsProbeServiceTest, Expired) {
  service_.set_mock_results(DnsProbeJob::SERVERS_CORRECT,
                            DnsProbeJob::SERVERS_CORRECT);

  Probe();
  EXPECT_TRUE(service_.jobs_created());

  RunUntilIdle();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(chrome_common_net::DNS_PROBE_NXDOMAIN, callback_result_);

  Reset();

  service_.MockExpireResults();

  Probe();
  EXPECT_TRUE(service_.jobs_created());

  RunUntilIdle();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(chrome_common_net::DNS_PROBE_NXDOMAIN, callback_result_);
}

TEST_F(DnsProbeServiceTest, SystemFail) {
  service_.set_mock_results(DnsProbeJob::SERVERS_CORRECT,
                            DnsProbeJob::SERVERS_CORRECT);
  service_.set_mock_system_fail(true);

  Probe();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(chrome_common_net::DNS_PROBE_UNKNOWN, callback_result_);

  Reset();

  RunUntilIdle();
  EXPECT_FALSE(callback_called_);
}

}  // namespace

}  // namespace chrome_browser_net
