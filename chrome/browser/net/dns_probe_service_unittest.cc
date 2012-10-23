// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_service.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/net/dns_probe_job.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_browser_net {

namespace {

class MockDnsProbeJob : public DnsProbeJob {
 public:
  MockDnsProbeJob(const CallbackType& callback,
                  Result result) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, base::Unretained(this), result));
  }

  virtual ~MockDnsProbeJob() { }
};

class TestDnsProbeService : public DnsProbeService {
 public:
  TestDnsProbeService()
      : DnsProbeService(),
        system_job_created_(false),
        public_job_created_(false),
        mock_system_result_(DnsProbeJob::DNS_UNKNOWN),
        mock_public_result_(DnsProbeJob::DNS_UNKNOWN) {
  }

  virtual ~TestDnsProbeService() { }

  void SetMockJobResults(
      DnsProbeJob::Result mock_system_result,
      DnsProbeJob::Result mock_public_result) {
    mock_system_result_ = mock_system_result;
    mock_public_result_ = mock_public_result;
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
};

class DnsProbeServiceTest : public testing::Test {
 public:
  DnsProbeServiceTest()
      : callback_called_(false),
        callback_result_(DnsProbeService::DNS_PROBE_UNKNOWN) {
  }

  void SetMockJobResults(DnsProbeJob::Result mock_system_result,
                         DnsProbeJob::Result mock_public_result) {
    service_.SetMockJobResults(mock_system_result, mock_public_result);
  }

  void Probe() {
    service_.ProbeDns(base::Bind(&DnsProbeServiceTest::ProbeCallback,
                                  base::Unretained(this)));
  }

  void RunUntilIdle() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  void ExpireResults() {
    service_.MockExpireResults();
  }

  MessageLoopForIO message_loop_;
  TestDnsProbeService service_;
  bool callback_called_;
  DnsProbeService::Result callback_result_;

 private:
  void ProbeCallback(DnsProbeService::Result result) {
    callback_called_ = true;
    callback_result_ = result;
  }
};

TEST_F(DnsProbeServiceTest, Null) {
}

TEST_F(DnsProbeServiceTest, Probe) {
  SetMockJobResults(DnsProbeJob::DNS_WORKING, DnsProbeJob::DNS_WORKING);

  Probe();
  EXPECT_TRUE(service_.system_job_created_);
  EXPECT_TRUE(service_.public_job_created_);
  EXPECT_FALSE(callback_called_);

  RunUntilIdle();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(DnsProbeService::DNS_PROBE_NXDOMAIN, callback_result_);
}

TEST_F(DnsProbeServiceTest, Cache) {
  SetMockJobResults(DnsProbeJob::DNS_WORKING, DnsProbeJob::DNS_WORKING);

  Probe();
  EXPECT_TRUE(service_.system_job_created_);
  EXPECT_TRUE(service_.public_job_created_);

  RunUntilIdle();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(DnsProbeService::DNS_PROBE_NXDOMAIN, callback_result_);

  callback_called_ = false;
  service_.system_job_created_ = false;
  service_.public_job_created_ = false;

  Probe();
  EXPECT_FALSE(service_.system_job_created_);
  EXPECT_FALSE(service_.public_job_created_);

  RunUntilIdle();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(DnsProbeService::DNS_PROBE_NXDOMAIN, callback_result_);
}

TEST_F(DnsProbeServiceTest, Expired) {
  SetMockJobResults(DnsProbeJob::DNS_WORKING, DnsProbeJob::DNS_WORKING);

  Probe();
  EXPECT_TRUE(service_.system_job_created_);
  EXPECT_TRUE(service_.public_job_created_);

  RunUntilIdle();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(DnsProbeService::DNS_PROBE_NXDOMAIN, callback_result_);

  callback_called_ = false;
  service_.system_job_created_ = false;
  service_.public_job_created_ = false;

  ExpireResults();

  Probe();
  EXPECT_TRUE(service_.system_job_created_);
  EXPECT_TRUE(service_.public_job_created_);

  RunUntilIdle();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(DnsProbeService::DNS_PROBE_NXDOMAIN, callback_result_);
}

}  // namespace

}  // namespace chrome_browser_net
