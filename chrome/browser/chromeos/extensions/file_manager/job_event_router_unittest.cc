// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/job_event_router.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace {

class JobEventRouterImpl : public JobEventRouter {
 public:
  JobEventRouterImpl() : JobEventRouter(base::TimeDelta::FromMilliseconds(0)) {}
  std::vector<linked_ptr<base::DictionaryValue>> events;

 protected:
  GURL ConvertDrivePathToFileSystemUrl(const base::FilePath& path,
                                       const std::string& id) const override {
    return GURL();
  }

  void BroadcastEvent(const std::string& event_name,
                      scoped_ptr<base::ListValue> event_args) override {
    ASSERT_EQ(1u, event_args->GetSize());
    const base::DictionaryValue* event;
    event_args->GetDictionary(0, &event);
    events.push_back(make_linked_ptr(event->DeepCopy()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(JobEventRouterImpl);
};

class JobEventRouterTest : public testing::Test {
 protected:
  void SetUp() override { job_event_router.reset(new JobEventRouterImpl()); }

  drive::JobInfo CreateJobInfo(drive::JobID id,
                               int64 num_completed_bytes,
                               int64 num_total_bytes) {
    drive::JobInfo job(drive::TYPE_DOWNLOAD_FILE);
    job.job_id = id;
    job.num_total_bytes = num_total_bytes;
    job.num_completed_bytes = num_completed_bytes;
    return job;
  }

  std::string GetEventString(size_t index, const std::string& name) {
    std::string value;
    job_event_router->events[index]->GetString(name, &value);
    return value;
  }

  double GetEventDouble(size_t index, const std::string& name) {
    double value = NAN;
    job_event_router->events[index]->GetDouble(name, &value);
    return value;
  }

  scoped_ptr<JobEventRouterImpl> job_event_router;

 private:
  base::MessageLoop message_loop_;
};

TEST_F(JobEventRouterTest, Basic) {
  // Add a job.
  job_event_router->OnJobAdded(CreateJobInfo(0, 0, 100));
  // Event should be throttled.
  ASSERT_EQ(0u, job_event_router->events.size());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, job_event_router->events.size());
  EXPECT_EQ("in_progress", GetEventString(0, "transferState"));
  EXPECT_EQ(0.0f, GetEventDouble(0, "processed"));
  EXPECT_EQ(100.0f, GetEventDouble(0, "total"));
  job_event_router->events.clear();

  // Job is updated.
  job_event_router->OnJobUpdated(CreateJobInfo(0, 50, 100));
  job_event_router->OnJobUpdated(CreateJobInfo(0, 100, 100));
  // Event should be throttled.
  ASSERT_EQ(0u, job_event_router->events.size());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, job_event_router->events.size());
  EXPECT_EQ("in_progress", GetEventString(0, "transferState"));
  EXPECT_EQ(100.0f, GetEventDouble(0, "processed"));
  EXPECT_EQ(100.0f, GetEventDouble(0, "total"));
  job_event_router->events.clear();

  // Complete first job.
  job_event_router->OnJobDone(CreateJobInfo(0, 100, 100), drive::FILE_ERROR_OK);
  // Complete event should not be throttled.
  ASSERT_EQ(1u, job_event_router->events.size());
  EXPECT_EQ("completed", GetEventString(0, "transferState"));
  EXPECT_EQ(100.0f, GetEventDouble(0, "processed"));
  EXPECT_EQ(100.0f, GetEventDouble(0, "total"));
  job_event_router->events.clear();
}

TEST_F(JobEventRouterTest, CompleteWithInvalidCompletedBytes) {
  job_event_router->OnJobDone(CreateJobInfo(0, 50, 100), drive::FILE_ERROR_OK);
  ASSERT_EQ(1u, job_event_router->events.size());
  EXPECT_EQ("completed", GetEventString(0, "transferState"));
  EXPECT_EQ(100.0f, GetEventDouble(0, "processed"));
  EXPECT_EQ(100.0f, GetEventDouble(0, "total"));
}

TEST_F(JobEventRouterTest, AnotherJobAddedBeforeComplete) {
  job_event_router->OnJobAdded(CreateJobInfo(0, 0, 100));
  job_event_router->OnJobUpdated(CreateJobInfo(0, 50, 100));
  job_event_router->OnJobAdded(CreateJobInfo(1, 0, 100));

  // Event should be throttled.
  ASSERT_EQ(0u, job_event_router->events.size());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, job_event_router->events.size());
  EXPECT_EQ("in_progress", GetEventString(0, "transferState"));
  EXPECT_EQ(50.0f, GetEventDouble(0, "processed"));
  EXPECT_EQ(200.0f, GetEventDouble(0, "total"));
  job_event_router->events.clear();

  job_event_router->OnJobDone(CreateJobInfo(0, 100, 100), drive::FILE_ERROR_OK);
  job_event_router->OnJobDone(CreateJobInfo(1, 100, 100), drive::FILE_ERROR_OK);
  // Complete event should not be throttled.
  ASSERT_EQ(2u, job_event_router->events.size());
  EXPECT_EQ("completed", GetEventString(0, "transferState"));
  EXPECT_EQ(100.0f, GetEventDouble(0, "processed"));
  EXPECT_EQ(200.0f, GetEventDouble(0, "total"));
  EXPECT_EQ("completed", GetEventString(1, "transferState"));
  EXPECT_EQ(200.0f, GetEventDouble(1, "processed"));
  EXPECT_EQ(200.0f, GetEventDouble(1, "total"));
}

TEST_F(JobEventRouterTest, AnotherJobAddedAfterComplete) {
  job_event_router->OnJobAdded(CreateJobInfo(0, 0, 100));
  job_event_router->OnJobUpdated(CreateJobInfo(0, 50, 100));
  job_event_router->OnJobDone(CreateJobInfo(0, 100, 100), drive::FILE_ERROR_OK);
  job_event_router->OnJobAdded(CreateJobInfo(1, 0, 100));
  job_event_router->OnJobDone(CreateJobInfo(1, 100, 100), drive::FILE_ERROR_OK);

  // Complete event should not be throttled.
  ASSERT_EQ(2u, job_event_router->events.size());
  EXPECT_EQ("completed", GetEventString(0, "transferState"));
  EXPECT_EQ(100.0f, GetEventDouble(0, "processed"));
  // Total byte shold be reset when all tasks complete.
  EXPECT_EQ(100.0f, GetEventDouble(0, "total"));
  EXPECT_EQ("completed", GetEventString(1, "transferState"));
  EXPECT_EQ(100.0f, GetEventDouble(1, "processed"));
  EXPECT_EQ(100.0f, GetEventDouble(1, "total"));
}

TEST_F(JobEventRouterTest, UpdateTotalSizeAfterAdded) {
  job_event_router->OnJobAdded(CreateJobInfo(0, 0, 0));
  base::RunLoop().RunUntilIdle();
  job_event_router->OnJobUpdated(CreateJobInfo(0, 0, 100));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2u, job_event_router->events.size());

  EXPECT_EQ("in_progress", GetEventString(0, "transferState"));
  EXPECT_EQ(0.0f, GetEventDouble(0, "processed"));
  EXPECT_EQ(0.0f, GetEventDouble(0, "total"));

  EXPECT_EQ("in_progress", GetEventString(1, "transferState"));
  EXPECT_EQ(0.0f, GetEventDouble(1, "processed"));
  EXPECT_EQ(100.0f, GetEventDouble(1, "total"));
}

}  // namespace
}  // namespace file_manager
