// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/test_util.h"

#include "base/bind.h"
#include "base/callback.h"
#include "components/domain_reliability/scheduler.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {

namespace {

class MockTimer : public MockableTime::Timer {
 public:
  MockTimer(MockTime* time)
      : time_(time),
        running_(false),
        callback_sequence_number_(0),
        weak_factory_(this) {
    DCHECK(time);
  }

  virtual ~MockTimer() {}

  // MockableTime::Timer implementation:
  virtual void Start(const tracked_objects::Location& posted_from,
                     base::TimeDelta delay,
                     const base::Closure& user_task) OVERRIDE {
    DCHECK(!user_task.is_null());

    if (running_)
      ++callback_sequence_number_;
    running_ = true;
    user_task_ = user_task;
    time_->AddTask(delay,
        base::Bind(&MockTimer::OnDelayPassed,
                   weak_factory_.GetWeakPtr(),
                   callback_sequence_number_));
  }

  virtual void Stop() OVERRIDE {
    if (running_) {
      ++callback_sequence_number_;
      running_ = false;
    }
  }

  virtual bool IsRunning() OVERRIDE { return running_; }

 private:
  void OnDelayPassed(int expected_callback_sequence_number) {
    if (callback_sequence_number_ != expected_callback_sequence_number)
      return;

    DCHECK(running_);
    running_ = false;

    // Grab user task in case it re-entrantly starts the timer again.
    base::Closure task_to_run = user_task_;
    user_task_.Reset();
    task_to_run.Run();
  }

  MockTime* time_;
  bool running_;
  int callback_sequence_number_;
  base::Closure user_task_;
  base::WeakPtrFactory<MockTimer> weak_factory_;
};

}  // namespace

TestCallback::TestCallback()
    : callback_(base::Bind(&TestCallback::OnCalled,
                           base::Unretained(this))),
      called_(false) {}

TestCallback::~TestCallback() {}

void TestCallback::OnCalled() {
  EXPECT_FALSE(called_);
  called_ = true;
}

MockUploader::MockUploader(const UploadRequestCallback& callback)
    : callback_(callback) {}

MockUploader::~MockUploader() {}

void MockUploader::UploadReport(const std::string& report_json,
                                const GURL& upload_url,
                                const UploadCallback& callback) {
  callback_.Run(report_json, upload_url, callback);
}

MockTime::MockTime()
    : now_(base::Time::Now()),
      now_ticks_(base::TimeTicks::Now()),
      epoch_ticks_(now_ticks_),
      task_sequence_number_(0) {
  VLOG(1) << "Creating mock time: T=" << elapsed_sec() << "s";
}

MockTime::~MockTime() {}

base::Time MockTime::Now() { return now_; }
base::TimeTicks MockTime::NowTicks() { return now_ticks_; }

scoped_ptr<MockableTime::Timer> MockTime::CreateTimer() {
  return scoped_ptr<MockableTime::Timer>(new MockTimer(this));
}

void MockTime::Advance(base::TimeDelta delta) {
  base::TimeTicks target_ticks = now_ticks_ + delta;

  while (!tasks_.empty() && tasks_.begin()->first.time <= target_ticks) {
    TaskKey key = tasks_.begin()->first;
    base::Closure task = tasks_.begin()->second;
    tasks_.erase(tasks_.begin());

    DCHECK(now_ticks_ <= key.time);
    DCHECK(key.time <= target_ticks);
    AdvanceToInternal(key.time);
    VLOG(1) << "Advancing mock time: task at T=" << elapsed_sec() << "s";

    task.Run();
  }

  DCHECK(now_ticks_ <= target_ticks);
  AdvanceToInternal(target_ticks);
  VLOG(1) << "Advanced mock time: T=" << elapsed_sec() << "s";
}

void MockTime::AddTask(base::TimeDelta delay, const base::Closure& task) {
  tasks_[TaskKey(now_ticks_ + delay, task_sequence_number_++)] = task;
}

void MockTime::AdvanceToInternal(base::TimeTicks target_ticks) {
  base::TimeDelta delta = target_ticks - now_ticks_;
  now_ += delta;
  now_ticks_ += delta;
}

DomainReliabilityScheduler::Params MakeTestSchedulerParams() {
  DomainReliabilityScheduler::Params params;
  params.minimum_upload_delay = base::TimeDelta::FromMinutes(1);
  params.maximum_upload_delay = base::TimeDelta::FromMinutes(5);
  params.upload_retry_interval = base::TimeDelta::FromSeconds(15);
  return params;
}

scoped_ptr<const DomainReliabilityConfig> MakeTestConfig() {
  return MakeTestConfigWithDomain("example");
}

scoped_ptr<const DomainReliabilityConfig> MakeTestConfigWithDomain(
    const std::string& domain) {
  DomainReliabilityConfig* config = new DomainReliabilityConfig();
  DomainReliabilityConfig::Resource* resource;

  resource = new DomainReliabilityConfig::Resource();
  resource->name = "always_report";
  resource->url_patterns.push_back(
      new std::string("http://*/always_report"));
  resource->success_sample_rate = 1.0;
  resource->failure_sample_rate = 1.0;
  config->resources.push_back(resource);

  resource = new DomainReliabilityConfig::Resource();
  resource->name = "never_report";
  resource->url_patterns.push_back(
      new std::string("http://*/never_report"));
  resource->success_sample_rate = 0.0;
  resource->failure_sample_rate = 0.0;
  config->resources.push_back(resource);

  DomainReliabilityConfig::Collector* collector;
  collector = new DomainReliabilityConfig::Collector();
  collector->upload_url = GURL("https://exampleuploader/upload");
  config->collectors.push_back(collector);

  config->version = "1";
  config->valid_until = 1234567890.0;
  config->domain = domain;

  DCHECK(config->IsValid());

  return scoped_ptr<const DomainReliabilityConfig>(config);
}

}  // namespace domain_reliability
