// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_TEST_UTIL_H_
#define COMPONENTS_DOMAIN_RELIABILITY_TEST_UTIL_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/scheduler.h"
#include "components/domain_reliability/uploader.h"
#include "components/domain_reliability/util.h"
#include "net/base/host_port_pair.h"

namespace net {
class URLRequestStatus;
}  // namespace net

namespace domain_reliability {

// A simple test callback that remembers whether it's been called.
class TestCallback {
 public:
  TestCallback();
  ~TestCallback();

  // Returns a callback that can be called only once.
  const base::Closure& callback() const { return callback_; }
  // Returns whether the callback returned by |callback()| has been called.
  bool called() const { return called_; }

 private:
  void OnCalled();

  base::Closure callback_;
  bool called_;
};

class MockUploader : public DomainReliabilityUploader {
 public:
  typedef base::Callback<void(const std::string& report_json,
                              const GURL& upload_url,
                              const UploadCallback& upload_callback)>
      UploadRequestCallback;

  MockUploader(const UploadRequestCallback& callback);

  virtual ~MockUploader();

  virtual bool discard_uploads() const;

  // DomainReliabilityUploader implementation:
  virtual void UploadReport(const std::string& report_json,
                            const GURL& upload_url,
                            const UploadCallback& callback) OVERRIDE;

  virtual void set_discard_uploads(bool discard_uploads) OVERRIDE;

 private:
  UploadRequestCallback callback_;
  bool discard_uploads_;
};

class MockTime : public MockableTime {
 public:
  MockTime();

  // N.B.: Tasks (and therefore Timers) scheduled to run in the future will
  // never be run if MockTime is destroyed before the mock time is advanced
  // to their scheduled time.
  virtual ~MockTime();

  // MockableTime implementation:
  virtual base::Time Now() OVERRIDE;
  virtual base::TimeTicks NowTicks() OVERRIDE;
  virtual scoped_ptr<MockableTime::Timer> CreateTimer() OVERRIDE;

  // Pretends that |delta| has passed, and runs tasks that would've happened
  // during that interval (with |Now()| returning proper values while they
  // execute!)
  void Advance(base::TimeDelta delta);

  // Queues |task| to be run after |delay|. (Lighter-weight than mocking an
  // entire message pump.)
  void AddTask(base::TimeDelta delay, const base::Closure& task);

 private:
  // Key used to store tasks in the task map. Includes the time the task should
  // run and a sequence number to disambiguate tasks with the same time.
  struct TaskKey {
    TaskKey(base::TimeTicks time, int sequence_number)
        : time(time),
          sequence_number(sequence_number) {}

    base::TimeTicks time;
    int sequence_number;
  };

  // Comparator for TaskKey; sorts by time, then by sequence number.
  struct TaskKeyCompare {
    bool operator() (const TaskKey& lhs, const TaskKey& rhs) const {
      return lhs.time < rhs.time ||
             (lhs.time == rhs.time &&
              lhs.sequence_number < rhs.sequence_number);
    }
  };

  typedef std::map<TaskKey, base::Closure, TaskKeyCompare> TaskMap;

  void AdvanceToInternal(base::TimeTicks target_ticks);

  int elapsed_sec() { return (now_ticks_ - epoch_ticks_).InSeconds(); }

  base::Time now_;
  base::TimeTicks now_ticks_;
  base::TimeTicks epoch_ticks_;
  int task_sequence_number_;
  TaskMap tasks_;
};

scoped_ptr<const DomainReliabilityConfig> MakeTestConfig();
scoped_ptr<const DomainReliabilityConfig> MakeTestConfigWithDomain(
    const std::string& domain);
DomainReliabilityScheduler::Params MakeTestSchedulerParams();

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_TEST_UTIL_H_
