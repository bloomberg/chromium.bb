// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/media_perception_private/media_perception_api_manager.h"

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_media_analytics_client.h"
#include "chromeos/dbus/fake_upstart_client.h"
#include "chromeos/dbus/media_analytics_client.h"
#include "chromeos/dbus/upstart_client.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_perception = extensions::api::media_perception_private;

namespace chromeos {
namespace {

class TestUpstartClient : public FakeUpstartClient {
 public:
  TestUpstartClient() : enqueue_requests_(false) {}

  ~TestUpstartClient() override {}

  // Overrides behavior to queue start requests.
  void StartMediaAnalytics(const UpstartCallback& callback) override {
    HandleUpstartRequest(callback);
  }

  // Overrides behavior to queue restart requests.
  void RestartMediaAnalytics(const UpstartCallback& callback) override {
    HandleUpstartRequest(callback);
  }

  // Triggers the next queue'd start request to succeed or fail.
  bool HandleNextUpstartRequest(bool should_succeed) {
    if (pending_upstart_request_callbacks_.empty())
      return false;

    UpstartCallback callback = pending_upstart_request_callbacks_.front();
    pending_upstart_request_callbacks_.pop();

    if (!should_succeed) {
      callback.Run(false);
      return true;
    }

    FakeUpstartClient::StartMediaAnalytics(callback);
    return true;
  }

  void set_enqueue_requests(bool enqueue_requests) {
    enqueue_requests_ = enqueue_requests;
  }

 private:
  void HandleUpstartRequest(const UpstartCallback& callback) {
    pending_upstart_request_callbacks_.push(callback);
    if (!enqueue_requests_) {
      HandleNextUpstartRequest(true);
    }
  }

  base::queue<UpstartCallback> pending_upstart_request_callbacks_;

  bool enqueue_requests_;

  DISALLOW_COPY_AND_ASSIGN(TestUpstartClient);
};

}  // namespace
}  // namespace chromeos

namespace extensions {

namespace {

void RecordServiceErrorFromStateAndRunClosure(
    base::Closure quit_run_loop,
    media_perception::ServiceError* service_error,
    media_perception::State result_state) {
  *service_error = result_state.service_error;
  quit_run_loop.Run();
}

void RecordServiceErrorFromDiagnosticsAndRunClosure(
    base::Closure quit_run_loop,
    media_perception::ServiceError* service_error,
    media_perception::Diagnostics result_diagnostics) {
  *service_error = result_diagnostics.service_error;
  quit_run_loop.Run();
}

media_perception::ServiceError SetStateAndWaitForResponse(
    MediaPerceptionAPIManager* manager,
    const media_perception::State& state) {
  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager->SetState(state, base::Bind(&RecordServiceErrorFromStateAndRunClosure,
                                      run_loop.QuitClosure(), &service_error));
  run_loop.Run();
  return service_error;
}

media_perception::ServiceError GetStateAndWaitForResponse(
    MediaPerceptionAPIManager* manager) {
  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager->GetState(base::Bind(&RecordServiceErrorFromStateAndRunClosure,
                               run_loop.QuitClosure(), &service_error));
  run_loop.Run();
  return service_error;
}

media_perception::ServiceError GetDiagnosticsAndWaitForResponse(
    MediaPerceptionAPIManager* manager) {
  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager->GetDiagnostics(
      base::Bind(&RecordServiceErrorFromDiagnosticsAndRunClosure,
                 run_loop.QuitClosure(), &service_error));
  run_loop.Run();
  return service_error;
}

}  // namespace

class MediaPerceptionAPIManagerTest : public testing::Test {
 public:
  MediaPerceptionAPIManagerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::DEFAULT) {}

  void SetUp() override {
    std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    auto media_analytics_client =
        std::make_unique<chromeos::FakeMediaAnalyticsClient>();
    media_analytics_client_ = media_analytics_client.get();
    dbus_setter->SetMediaAnalyticsClient(std::move(media_analytics_client));

    auto upstart_client = std::make_unique<chromeos::TestUpstartClient>();
    upstart_client_ = upstart_client.get();
    dbus_setter->SetUpstartClient(std::move(upstart_client));

    manager_ = std::make_unique<MediaPerceptionAPIManager>(&browser_context_);
  }

  void TearDown() override {
    // Need to make sure that the MediaPerceptionAPIManager is destructed before
    // the DbusThreadManager.
    manager_.reset();
    chromeos::DBusThreadManager::Shutdown();
  }

  std::unique_ptr<MediaPerceptionAPIManager> manager_;

  // Ownership of both is passed on to chromeos::DbusThreadManager.
  chromeos::FakeMediaAnalyticsClient* media_analytics_client_;
  chromeos::TestUpstartClient* upstart_client_;

 private:
  content::TestBrowserContext browser_context_;
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(MediaPerceptionAPIManagerTest);
};

TEST_F(MediaPerceptionAPIManagerTest, UpstartFailure) {
  upstart_client_->set_enqueue_requests(true);
  media_perception::State state;
  state.status = media_perception::STATUS_RUNNING;

  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager_->SetState(state,
                     base::Bind(&RecordServiceErrorFromStateAndRunClosure,
                                run_loop.QuitClosure(), &service_error));
  EXPECT_TRUE(upstart_client_->HandleNextUpstartRequest(false));
  run_loop.Run();
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_NOT_RUNNING, service_error);

  // Check that after a failed request, setState RUNNING will go through.
  upstart_client_->set_enqueue_requests(false);
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            SetStateAndWaitForResponse(manager_.get(), state));
}

TEST_F(MediaPerceptionAPIManagerTest, UpstartRestartFailure) {
  upstart_client_->set_enqueue_requests(true);
  media_perception::State state;
  state.status = media_perception::STATUS_RESTARTING;

  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager_->SetState(state,
                     base::Bind(&RecordServiceErrorFromStateAndRunClosure,
                                run_loop.QuitClosure(), &service_error));
  EXPECT_TRUE(upstart_client_->HandleNextUpstartRequest(false));
  run_loop.Run();
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_NOT_RUNNING, service_error);

  // Check that after a failed request, setState restarted will still go
  // through.
  upstart_client_->set_enqueue_requests(false);
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            SetStateAndWaitForResponse(manager_.get(), state));
}

TEST_F(MediaPerceptionAPIManagerTest, UpstartStall) {
  upstart_client_->set_enqueue_requests(true);
  media_perception::State state;
  state.status = media_perception::STATUS_RUNNING;

  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager_->SetState(state,
                     base::Bind(&RecordServiceErrorFromStateAndRunClosure,
                                run_loop.QuitClosure(), &service_error));

  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_BUSY_LAUNCHING,
            GetStateAndWaitForResponse(manager_.get()));
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_BUSY_LAUNCHING,
            SetStateAndWaitForResponse(manager_.get(), state));
  EXPECT_TRUE(upstart_client_->HandleNextUpstartRequest(true));
  run_loop.Run();
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE, service_error);

  // Verify that after the slow start, things works as normal.
  upstart_client_->set_enqueue_requests(false);
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            GetStateAndWaitForResponse(manager_.get()));
  state.status = media_perception::STATUS_SUSPENDED;
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            SetStateAndWaitForResponse(manager_.get(), state));
}

TEST_F(MediaPerceptionAPIManagerTest, UpstartRestartStall) {
  upstart_client_->set_enqueue_requests(true);
  media_perception::State state;
  state.status = media_perception::STATUS_RESTARTING;

  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager_->SetState(state,
                     base::Bind(&RecordServiceErrorFromStateAndRunClosure,
                                run_loop.QuitClosure(), &service_error));

  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_BUSY_LAUNCHING,
            GetStateAndWaitForResponse(manager_.get()));
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_BUSY_LAUNCHING,
            SetStateAndWaitForResponse(manager_.get(), state));
  EXPECT_TRUE(upstart_client_->HandleNextUpstartRequest(true));
  run_loop.Run();
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE, service_error);

  // Verify that after the slow start, things works as normal.
  upstart_client_->set_enqueue_requests(false);
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            GetStateAndWaitForResponse(manager_.get()));
  state.status = media_perception::STATUS_RUNNING;
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            SetStateAndWaitForResponse(manager_.get(), state));
}

TEST_F(MediaPerceptionAPIManagerTest, MediaAnalyticsDbusError) {
  media_perception::State state;
  state.status = media_perception::STATUS_RUNNING;
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            SetStateAndWaitForResponse(manager_.get(), state));
  // Disable the functionality of the fake process.
  media_analytics_client_->set_process_running(false);
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_UNREACHABLE,
            GetStateAndWaitForResponse(manager_.get()));
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_UNREACHABLE,
            SetStateAndWaitForResponse(manager_.get(), state));
  // Check that getting diagnostics also errors in the same way.
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_UNREACHABLE,
            GetDiagnosticsAndWaitForResponse(manager_.get()));
}

}  // namespace extensions
