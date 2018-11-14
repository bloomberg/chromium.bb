// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/service.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/domain_reliability/monitor.h"
#include "components/domain_reliability/test_util.h"
#include "net/base/host_port_pair.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {


class DomainReliabilityServiceTest : public testing::Test {
 protected:
  using RequestInfo = DomainReliabilityMonitor::RequestInfo;

  DomainReliabilityServiceTest() : upload_reporter_string_("test") {
    url_request_context_getter_ = new net::TestURLRequestContextGetter(
        scoped_task_environment_.GetMainThreadTaskRunner());
    service_ = base::WrapUnique(
        DomainReliabilityService::Create(upload_reporter_string_));
    monitor_ = service_->CreateMonitor(
        scoped_task_environment_.GetMainThreadTaskRunner(),
        scoped_task_environment_.GetMainThreadTaskRunner(),
        base::BindRepeating(
            &DomainReliabilityServiceTest::CheckDomainReliablityUploadAllowed,
            base::Unretained(this)));
    monitor_->InitializeOnNetworkThread();
    monitor_->InitURLRequestContext(url_request_context_getter_);
    monitor_->SetDiscardUploads(true);
  }

  ~DomainReliabilityServiceTest() override {
    if (monitor_)
      monitor_->Shutdown();
  }

  void OnRequestLegComplete(RequestInfo request) {
    monitor_->OnRequestLegComplete(request);
  }

  int GetDiscardedUploadCount() const {
    return monitor_->uploader_->GetDiscardedUploadCount();
  }

  void set_upload_allowed(bool allowed) { upload_allowed_ = allowed; }

  int upload_check_count() { return upload_check_count_; }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::string upload_reporter_string_;

  std::unique_ptr<DomainReliabilityService> service_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  std::unique_ptr<DomainReliabilityMonitor> monitor_;

 private:
  void CheckDomainReliablityUploadAllowed(
      const GURL& origin,
      base::OnceCallback<void(bool)> callback) {
    ++upload_check_count_;
    std::move(callback).Run(upload_allowed_);
  }

  bool upload_allowed_ = false;
  int upload_check_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityServiceTest);
};

namespace {

TEST_F(DomainReliabilityServiceTest, Create) {}

TEST_F(DomainReliabilityServiceTest, UploadAllowed) {
  set_upload_allowed(true);

  monitor_->AddContextForTesting(
      MakeTestConfigWithOrigin(GURL("https://example/")));

  RequestInfo request;
  request.status =
      net::URLRequestStatus::FromError(net::ERR_CONNECTION_REFUSED);
  request.response_info.socket_address =
      net::HostPortPair::FromString("1.2.3.4");
  request.url = GURL("https://example/");
  request.response_info.was_cached = false;
  request.response_info.network_accessed = true;
  request.response_info.was_fetched_via_proxy = false;
  request.load_flags = 0;
  request.load_timing_info.request_start = base::TimeTicks::Now();
  request.upload_depth = 0;

  OnRequestLegComplete(request);

  monitor_->ForceUploadsForTesting();

  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1, upload_check_count());
  EXPECT_EQ(1, GetDiscardedUploadCount());
}

TEST_F(DomainReliabilityServiceTest, UploadForbidden) {
  set_upload_allowed(false);

  monitor_->AddContextForTesting(
      MakeTestConfigWithOrigin(GURL("https://example/")));

  RequestInfo request;
  request.status =
      net::URLRequestStatus::FromError(net::ERR_CONNECTION_REFUSED);
  request.response_info.socket_address =
      net::HostPortPair::FromString("1.2.3.4");
  request.url = GURL("https://example/");
  request.response_info.was_cached = false;
  request.response_info.network_accessed = true;
  request.response_info.was_fetched_via_proxy = false;
  request.load_flags = 0;
  request.load_timing_info.request_start = base::TimeTicks::Now();
  request.upload_depth = 0;

  OnRequestLegComplete(request);

  monitor_->ForceUploadsForTesting();

  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1, upload_check_count());
  EXPECT_EQ(0, GetDiscardedUploadCount());
}

TEST_F(DomainReliabilityServiceTest, MonitorDestroyedBeforeCheckRuns) {
  set_upload_allowed(false);

  monitor_->AddContextForTesting(
      MakeTestConfigWithOrigin(GURL("https://example/")));

  RequestInfo request;
  request.status =
      net::URLRequestStatus::FromError(net::ERR_CONNECTION_REFUSED);
  request.response_info.socket_address =
      net::HostPortPair::FromString("1.2.3.4");
  request.url = GURL("https://example/");
  request.response_info.was_cached = false;
  request.response_info.network_accessed = true;
  request.response_info.was_fetched_via_proxy = false;
  request.load_flags = 0;
  request.load_timing_info.request_start = base::TimeTicks::Now();
  request.upload_depth = 0;

  OnRequestLegComplete(request);

  monitor_->ForceUploadsForTesting();

  monitor_->Shutdown();
  monitor_.reset();

  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1, upload_check_count());
  // Makes no sense to check upload count, since monitor was destroyed.
}

TEST_F(DomainReliabilityServiceTest, MonitorDestroyedBeforeCheckReturns) {
  set_upload_allowed(false);

  monitor_->AddContextForTesting(
      MakeTestConfigWithOrigin(GURL("https://example/")));

  RequestInfo request;
  request.status =
      net::URLRequestStatus::FromError(net::ERR_CONNECTION_REFUSED);
  request.response_info.socket_address =
      net::HostPortPair::FromString("1.2.3.4");
  request.url = GURL("https://example/");
  request.response_info.was_cached = false;
  request.response_info.network_accessed = true;
  request.response_info.was_fetched_via_proxy = false;
  request.load_flags = 0;
  request.load_timing_info.request_start = base::TimeTicks::Now();
  request.upload_depth = 0;

  OnRequestLegComplete(request);

  monitor_->ForceUploadsForTesting();

  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1, upload_check_count());

  monitor_->Shutdown();
  monitor_.reset();

  scoped_task_environment_.RunUntilIdle();
  // Makes no sense to check upload count, since monitor was destroyed.
}

}  // namespace

}  // namespace domain_reliability
