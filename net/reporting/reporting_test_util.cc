// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_test_util.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/timer/mock_timer.h"
#include "net/base/rand_callback.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_client.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_delegate.h"
#include "net/reporting/reporting_delivery_agent.h"
#include "net/reporting/reporting_garbage_collector.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_uploader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

namespace {

class PendingUploadImpl : public TestReportingUploader::PendingUpload {
 public:
  PendingUploadImpl(const GURL& url,
                    const std::string& json,
                    ReportingUploader::UploadCallback callback,
                    base::OnceCallback<void(PendingUpload*)> complete_callback)
      : url_(url),
        json_(json),
        callback_(std::move(callback)),
        complete_callback_(std::move(complete_callback)) {}

  ~PendingUploadImpl() override = default;

  // PendingUpload implementationP:
  const GURL& url() const override { return url_; }
  const std::string& json() const override { return json_; }
  std::unique_ptr<base::Value> GetValue() const override {
    return base::JSONReader::Read(json_);
  }

  void Complete(ReportingUploader::Outcome outcome) override {
    std::move(callback_).Run(outcome);
    // Deletes |this|.
    std::move(complete_callback_).Run(this);
  }

 private:
  GURL url_;
  std::string json_;
  ReportingUploader::UploadCallback callback_;
  base::OnceCallback<void(PendingUpload*)> complete_callback_;
};

void ErasePendingUpload(
    std::vector<std::unique_ptr<TestReportingUploader::PendingUpload>>* uploads,
    TestReportingUploader::PendingUpload* upload) {
  for (auto it = uploads->begin(); it != uploads->end(); ++it) {
    if (it->get() == upload) {
      uploads->erase(it);
      return;
    }
  }
  NOTREACHED();
}

}  // namespace

const ReportingClient* FindClientInCache(const ReportingCache* cache,
                                         const url::Origin& origin,
                                         const GURL& endpoint) {
  std::vector<const ReportingClient*> clients;
  cache->GetClients(&clients);
  for (const ReportingClient* client : clients) {
    if (client->origin == origin && client->endpoint == endpoint)
      return client;
  }
  return nullptr;
}

TestReportingUploader::PendingUpload::~PendingUpload() = default;
TestReportingUploader::PendingUpload::PendingUpload() = default;

TestReportingUploader::TestReportingUploader() = default;
TestReportingUploader::~TestReportingUploader() = default;

void TestReportingUploader::StartUpload(const GURL& url,
                                        const std::string& json,
                                        UploadCallback callback) {
  pending_uploads_.push_back(std::make_unique<PendingUploadImpl>(
      url, json, std::move(callback),
      base::BindOnce(&ErasePendingUpload, &pending_uploads_)));
}

bool TestReportingUploader::RequestIsUpload(const URLRequest& request) {
  NOTIMPLEMENTED();
  return true;
}

TestReportingDelegate::TestReportingDelegate() = default;

TestReportingDelegate::~TestReportingDelegate() = default;

bool TestReportingDelegate::CanQueueReport(const url::Origin& origin) const {
  return true;
}

bool TestReportingDelegate::CanSendReport(const url::Origin& origin) const {
  return true;
}

bool TestReportingDelegate::CanSetClient(const url::Origin& origin,
                                         const GURL& endpoint) const {
  return true;
}

bool TestReportingDelegate::CanUseClient(const url::Origin& origin,
                                         const GURL& endpoint) const {
  return true;
}

TestReportingContext::TestReportingContext(base::Clock* clock,
                                           base::TickClock* tick_clock,
                                           const ReportingPolicy& policy)
    : ReportingContext(
          policy,
          clock,
          tick_clock,
          base::BindRepeating(&TestReportingContext::RandIntCallback,
                              base::Unretained(this)),
          std::make_unique<TestReportingUploader>(),
          std::make_unique<TestReportingDelegate>()),
      rand_counter_(0),
      delivery_timer_(new base::MockTimer(/* retain_user_task= */ false,
                                          /* is_repeating= */ false)),
      garbage_collection_timer_(
          new base::MockTimer(/* retain_user_task= */ false,
                              /* is_repeating= */ false)) {
  garbage_collector()->SetTimerForTesting(
      base::WrapUnique(garbage_collection_timer_));
  delivery_agent()->SetTimerForTesting(base::WrapUnique(delivery_timer_));
}

TestReportingContext::~TestReportingContext() {
  delivery_timer_ = nullptr;
  garbage_collection_timer_ = nullptr;
}

int TestReportingContext::RandIntCallback(int min, int max) {
  DCHECK_LE(min, max);
  return min + (rand_counter_++ % (max - min + 1));
}

ReportingTestBase::ReportingTestBase() {
  // For tests, disable jitter.
  ReportingPolicy policy;
  policy.endpoint_backoff_policy.jitter_factor = 0.0;

  CreateContext(policy, base::Time::Now(), base::TimeTicks::Now());
}

ReportingTestBase::~ReportingTestBase() = default;

void ReportingTestBase::UsePolicy(const ReportingPolicy& new_policy) {
  CreateContext(new_policy, clock()->Now(), tick_clock()->NowTicks());
}

void ReportingTestBase::SimulateRestart(base::TimeDelta delta,
                                        base::TimeDelta delta_ticks) {
  CreateContext(policy(), clock()->Now() + delta,
                tick_clock()->NowTicks() + delta_ticks);
}

void ReportingTestBase::CreateContext(const ReportingPolicy& policy,
                                      base::Time now,
                                      base::TimeTicks now_ticks) {
  context_ =
      std::make_unique<TestReportingContext>(&clock_, &tick_clock_, policy);
  clock()->SetNow(now);
  tick_clock()->SetNowTicks(now_ticks);
}

base::TimeTicks ReportingTestBase::yesterday() {
  return tick_clock()->NowTicks() - base::TimeDelta::FromDays(1);
}

base::TimeTicks ReportingTestBase::now() {
  return tick_clock()->NowTicks();
}

base::TimeTicks ReportingTestBase::tomorrow() {
  return tick_clock()->NowTicks() + base::TimeDelta::FromDays(1);
}

}  // namespace net
