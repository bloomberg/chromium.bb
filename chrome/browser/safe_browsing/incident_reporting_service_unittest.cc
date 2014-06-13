// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting_service.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_local.h"
#include "chrome/browser/safe_browsing/incident_report_uploader.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

// A test fixture that sets up a test task runner and makes it the thread's
// runner. The fixture implements a fake envrionment data collector and a fake
// report uploader.
class IncidentReportingServiceTest : public testing::Test {
 protected:
  // An IRS class that allows a test harness to provide a fake environment
  // collector and report uploader via callbacks.
  class TestIncidentReportingService
      : public safe_browsing::IncidentReportingService {
   public:
    typedef base::Callback<
        void(safe_browsing::ClientIncidentReport_EnvironmentData*)>
        CollectEnvironmentCallback;

    typedef base::Callback<scoped_ptr<safe_browsing::IncidentReportUploader>(
        const safe_browsing::IncidentReportUploader::OnResultCallback&,
        const safe_browsing::ClientIncidentReport& report)> StartUploadCallback;

    TestIncidentReportingService(
        const scoped_refptr<base::TaskRunner>& task_runner,
        const CollectEnvironmentCallback& collect_environment_callback,
        const StartUploadCallback& start_upload_callback)
        : IncidentReportingService(NULL, NULL),
          collect_environment_callback_(collect_environment_callback),
          start_upload_callback_(start_upload_callback) {
      SetCollectEnvironmentHook(&CollectEnvironmentData, task_runner);
      test_instance_.Get().Set(this);
    }

    virtual ~TestIncidentReportingService() { test_instance_.Get().Set(NULL); }

   protected:
    virtual scoped_ptr<safe_browsing::IncidentReportUploader> StartReportUpload(
        const safe_browsing::IncidentReportUploader::OnResultCallback& callback,
        const scoped_refptr<net::URLRequestContextGetter>&
            request_context_getter,
        const safe_browsing::ClientIncidentReport& report) OVERRIDE {
      return start_upload_callback_.Run(callback, report);
    }

   private:
    static TestIncidentReportingService& current() {
      return *test_instance_.Get().Get();
    }

    static void CollectEnvironmentData(
        safe_browsing::ClientIncidentReport_EnvironmentData* data) {
      current().collect_environment_callback_.Run(data);
    };

    static base::LazyInstance<base::ThreadLocalPointer<
        TestIncidentReportingService> >::Leaky test_instance_;

    CollectEnvironmentCallback collect_environment_callback_;
    StartUploadCallback start_upload_callback_;
  };

  static const char kFakeOsName[];

  IncidentReportingServiceTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        thread_task_runner_handle_(task_runner_),
        instance_(new TestIncidentReportingService(
            task_runner_,
            base::Bind(&IncidentReportingServiceTest::CollectEnvironmentData,
                       base::Unretained(this)),
            base::Bind(&IncidentReportingServiceTest::StartUpload,
                       base::Unretained(this)))),
        upload_result_(safe_browsing::IncidentReportUploader::UPLOAD_SUCCESS),
        environment_collected_(),
        uploader_destroyed_() {
    instance_->SetEnabled(true);
  }

  bool HasCollectedEnvironmentData() const { return environment_collected_; }
  bool UploaderDestroyed() const { return uploader_destroyed_; }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;
  scoped_ptr<safe_browsing::IncidentReportingService> instance_;
  safe_browsing::IncidentReportUploader::Result upload_result_;
  bool environment_collected_;
  scoped_ptr<safe_browsing::ClientIncidentReport> uploaded_report_;
  bool uploader_destroyed_;

 private:
  // A fake IncidentReportUploader that posts a task to provide a given response
  // back to the incident reporting service. It also reports back to the test
  // harness via a closure when it is deleted by the incident reporting service.
  class FakeUploader : public safe_browsing::IncidentReportUploader {
   public:
    FakeUploader(
        const base::Closure& on_deleted,
        const safe_browsing::IncidentReportUploader::OnResultCallback& callback,
        safe_browsing::IncidentReportUploader::Result result)
        : safe_browsing::IncidentReportUploader(callback),
          on_deleted_(on_deleted),
          result_(result) {
      // Post a task that will provide the response.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&FakeUploader::FinishUpload, base::Unretained(this)));
    }
    virtual ~FakeUploader() { on_deleted_.Run(); }

   private:
    void FinishUpload() {
      // Callbacks have a tendency to delete the uploader, so no touching
      // anything after this.
      callback_.Run(result_,
                    scoped_ptr<safe_browsing::ClientIncidentResponse>());
    }

    base::Closure on_deleted_;
    safe_browsing::IncidentReportUploader::Result result_;

    DISALLOW_COPY_AND_ASSIGN(FakeUploader);
  };

  void CollectEnvironmentData(
      safe_browsing::ClientIncidentReport_EnvironmentData* data) {
    ASSERT_NE(
        static_cast<safe_browsing::ClientIncidentReport_EnvironmentData*>(NULL),
        data);
    data->mutable_os()->set_os_name(kFakeOsName);
    environment_collected_ = true;
  }

  scoped_ptr<safe_browsing::IncidentReportUploader> StartUpload(
      const safe_browsing::IncidentReportUploader::OnResultCallback& callback,
      const safe_browsing::ClientIncidentReport& report) {
    // Remember the report that is being uploaded.
    uploaded_report_.reset(new safe_browsing::ClientIncidentReport(report));
    return scoped_ptr<safe_browsing::IncidentReportUploader>(new FakeUploader(
        base::Bind(&IncidentReportingServiceTest::OnUploaderDestroyed,
                   base::Unretained(this)),
        callback,
        upload_result_));
  }

  void OnUploaderDestroyed() { uploader_destroyed_ = true; }
};

// static
base::LazyInstance<base::ThreadLocalPointer<
    IncidentReportingServiceTest::TestIncidentReportingService> >::Leaky
    IncidentReportingServiceTest::TestIncidentReportingService::test_instance_ =
        LAZY_INSTANCE_INITIALIZER;

// static
const char IncidentReportingServiceTest::kFakeOsName[] = "fakedows";

TEST_F(IncidentReportingServiceTest, AddIncident) {
  // Make up a dummy incident.
  const int64 kIncidentTimeMsec = 47LL;
  scoped_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident(
      new safe_browsing::ClientIncidentReport_IncidentData());
  incident->set_incident_time_msec(kIncidentTimeMsec);

  // Add it to the service.
  instance_->GetAddIncidentCallback().Run(incident.Pass());

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that environment collection took place.
  EXPECT_TRUE(HasCollectedEnvironmentData());

  // Verify that report upload took place and contained the incident and
  // environment data.
  ASSERT_TRUE(uploaded_report_);
  ASSERT_EQ(1, uploaded_report_->incident_size());
  ASSERT_TRUE(uploaded_report_->incident(0).has_incident_time_msec());
  ASSERT_EQ(kIncidentTimeMsec,
            uploaded_report_->incident(0).incident_time_msec());
  ASSERT_TRUE(uploaded_report_->has_environment());
  ASSERT_TRUE(uploaded_report_->environment().has_os());
  ASSERT_TRUE(uploaded_report_->environment().os().has_os_name());
  ASSERT_EQ(std::string(kFakeOsName),
            uploaded_report_->environment().os().os_name());

  // Verify that the uploader was destroyed.
  ASSERT_TRUE(UploaderDestroyed());
}

// Nothing happens when disabled
// Nothing is sent when disabled before/after environment collection
// Duplicate incidents are stripped
// Parallel uploads
// Shutdown during processing
// environment colection taking longer than incident delay timer
// environment colection taking longer than incident delay timer, and then
// another incident arriving
