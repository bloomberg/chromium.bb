// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_background_services_context.h"

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "content/browser/devtools/devtools_background_services.pb.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace content {
namespace {

class TestBrowserClient : public ContentBrowserClient {
 public:
  TestBrowserClient() {}
  ~TestBrowserClient() override {}

  void UpdateDevToolsBackgroundServiceExpiration(
      BrowserContext* browser_context,
      int service,
      base::Time expiration_time) override {
    exp_dict_[service] = expiration_time;
  }

  base::flat_map<int, base::Time> GetDevToolsBackgroundServiceExpirations(
      BrowserContext* browser_context) override {
    return exp_dict_;
  }

 private:
  base::flat_map<int, base::Time> exp_dict_;
};

void DidRegisterServiceWorker(int64_t* out_service_worker_registration_id,
                              base::OnceClosure quit_closure,
                              blink::ServiceWorkerStatusCode status,
                              const std::string& status_message,
                              int64_t service_worker_registration_id) {
  DCHECK(out_service_worker_registration_id);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status) << status_message;

  *out_service_worker_registration_id = service_worker_registration_id;

  std::move(quit_closure).Run();
}

void DidFindServiceWorkerRegistration(
    scoped_refptr<ServiceWorkerRegistration>* out_service_worker_registration,
    base::OnceClosure quit_closure,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK(out_service_worker_registration);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status)
      << blink::ServiceWorkerStatusToString(status);

  *out_service_worker_registration = service_worker_registration;

  std::move(quit_closure).Run();
}

void DidGetLoggedBackgroundServiceEvents(
    base::OnceClosure quit_closure,
    std::vector<devtools::proto::BackgroundServiceState>* out_feature_states,
    std::vector<devtools::proto::BackgroundServiceState> feature_states) {
  *out_feature_states = std::move(feature_states);
  std::move(quit_closure).Run();
}

}  // namespace

class DevToolsBackgroundServicesContextTest : public ::testing::Test {
 public:
  DevToolsBackgroundServicesContextTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        embedded_worker_test_helper_(base::FilePath() /* in memory */) {}

  ~DevToolsBackgroundServicesContextTest() override = default;

  void SetUp() override {
    // Register Service Worker.
    service_worker_registration_id_ = RegisterServiceWorker();
    ASSERT_NE(service_worker_registration_id_,
              blink::mojom::kInvalidServiceWorkerRegistrationId);

    browser_client_ = std::make_unique<TestBrowserClient>();
    SetBrowserClientForTesting(browser_client_.get());

    SimulateBrowserRestart();
  }

 protected:
  void SimulateBrowserRestart() {
    // Create |context_|.
    context_ = base::MakeRefCounted<DevToolsBackgroundServicesContext>(
        &browser_context_, embedded_worker_test_helper_.context_wrapper());
    ASSERT_TRUE(context_);
  }

  base::Time GetExpirationTime() {
    return context_->expiration_times_
        [devtools::proto::BackgroundService::TEST_BACKGROUND_SERVICE];
  }

  std::vector<devtools::proto::BackgroundServiceState>
  GetLoggedBackgroundServiceEvents() {
    std::vector<devtools::proto::BackgroundServiceState> feature_states;

    base::RunLoop run_loop;
    context_->GetLoggedBackgroundServiceEvents(
        devtools::proto::BackgroundService::TEST_BACKGROUND_SERVICE,
        base::BindOnce(&DidGetLoggedBackgroundServiceEvents,
                       run_loop.QuitClosure(), &feature_states));
    run_loop.Run();

    return feature_states;
  }

  void LogTestBackgroundServiceEvent(const std::string& log_message) {
    devtools::proto::TestBackgroundServiceEvent event;
    event.set_value(log_message);

    context_->LogTestBackgroundServiceEvent(service_worker_registration_id_,
                                            origin_, std::move(event));
  }

  void StartRecording() {
    context_->StartRecording(
        devtools::proto::BackgroundService::TEST_BACKGROUND_SERVICE);

    // Wait for the messages to propagate to the browser client.
    thread_bundle_.RunUntilIdle();
  }

  void StopRecording() {
    context_->StopRecording(
        devtools::proto::BackgroundService::TEST_BACKGROUND_SERVICE);

    // Wait for the messages to propagate to the browser client.
    thread_bundle_.RunUntilIdle();
  }

  TestBrowserThreadBundle thread_bundle_;  // Must be first member.
  url::Origin origin_ = url::Origin::Create(GURL("https://example.com"));
  int64_t service_worker_registration_id_ =
      blink::mojom::kInvalidServiceWorkerRegistrationId;

 private:
  int64_t RegisterServiceWorker() {
    GURL script_url(origin_.GetURL().spec() + "sw.js");
    int64_t service_worker_registration_id =
        blink::mojom::kInvalidServiceWorkerRegistrationId;

    {
      blink::mojom::ServiceWorkerRegistrationOptions options;
      options.scope = origin_.GetURL();
      base::RunLoop run_loop;
      embedded_worker_test_helper_.context()->RegisterServiceWorker(
          script_url, options,
          base::BindOnce(&DidRegisterServiceWorker,
                         &service_worker_registration_id,
                         run_loop.QuitClosure()));

      run_loop.Run();
    }

    if (service_worker_registration_id ==
        blink::mojom::kInvalidServiceWorkerRegistrationId) {
      ADD_FAILURE() << "Could not obtain a valid Service Worker registration";
      return blink::mojom::kInvalidServiceWorkerRegistrationId;
    }

    {
      base::RunLoop run_loop;
      embedded_worker_test_helper_.context()->storage()->FindRegistrationForId(
          service_worker_registration_id, origin_.GetURL(),
          base::BindOnce(&DidFindServiceWorkerRegistration,
                         &service_worker_registration_,
                         run_loop.QuitClosure()));
      run_loop.Run();
    }

    // Wait for the worker to be activated.
    base::RunLoop().RunUntilIdle();

    if (!service_worker_registration_) {
      ADD_FAILURE() << "Could not find the new Service Worker registration.";
      return blink::mojom::kInvalidServiceWorkerRegistrationId;
    }

    return service_worker_registration_id;
  }

  EmbeddedWorkerTestHelper embedded_worker_test_helper_;
  TestBrowserContext browser_context_;
  scoped_refptr<DevToolsBackgroundServicesContext> context_;
  scoped_refptr<ServiceWorkerRegistration> service_worker_registration_;
  std::unique_ptr<ContentBrowserClient> browser_client_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsBackgroundServicesContextTest);
};

TEST_F(DevToolsBackgroundServicesContextTest,
       NothingStoredWithRecordingModeOff) {
  // Initially there are no entries.
  EXPECT_TRUE(GetLoggedBackgroundServiceEvents().empty());

  // "Log" some events and wait for them to finish.
  LogTestBackgroundServiceEvent("f1");
  LogTestBackgroundServiceEvent("f2");

  // There should still be nothing since recording mode is off.
  EXPECT_TRUE(GetLoggedBackgroundServiceEvents().empty());
}

TEST_F(DevToolsBackgroundServicesContextTest, GetLoggedFeatures) {
  StartRecording();

  // "Log" some events and wait for them to finish.
  LogTestBackgroundServiceEvent("f1");
  LogTestBackgroundServiceEvent("f2");

  // Check the values.
  auto feature_events = GetLoggedBackgroundServiceEvents();
  ASSERT_EQ(feature_events.size(), 2u);

  for (const auto& feature_event : feature_events) {
    EXPECT_EQ(feature_event.background_service(),
              devtools::proto::BackgroundService::TEST_BACKGROUND_SERVICE);
    EXPECT_EQ(feature_event.origin(), origin_.GetURL().spec());
    EXPECT_EQ(feature_event.service_worker_registration_id(),
              service_worker_registration_id_);
    ASSERT_TRUE(feature_event.has_test_event());
  }

  EXPECT_EQ(feature_events[0].test_event().value(), "f1");
  EXPECT_EQ(feature_events[1].test_event().value(), "f2");

  EXPECT_LE(feature_events[0].timestamp(), feature_events[1].timestamp());
}

TEST_F(DevToolsBackgroundServicesContextTest, StopRecording) {
  StartRecording();
  // Initially there are no entries.
  EXPECT_TRUE(GetLoggedBackgroundServiceEvents().empty());

  // "Log" some events and wait for them to finish.
  LogTestBackgroundServiceEvent("f1");
  StopRecording();
  LogTestBackgroundServiceEvent("f2");

  // Check the values.
  ASSERT_EQ(GetLoggedBackgroundServiceEvents().size(), 1u);
}

TEST_F(DevToolsBackgroundServicesContextTest, DelegateExpirationTimes) {
  // Initially expiration time is null.
  EXPECT_TRUE(GetExpirationTime().is_null());

  // Toggle Recording mode, and now this should be non-null.
  StartRecording();
  EXPECT_FALSE(GetExpirationTime().is_null());

  // The value should still be  there on browser restarts.
  SimulateBrowserRestart();
  EXPECT_FALSE(GetExpirationTime().is_null());

  // Stopping Recording mode should clear the value.
  StopRecording();
  EXPECT_TRUE(GetExpirationTime().is_null());
  SimulateBrowserRestart();
  EXPECT_TRUE(GetExpirationTime().is_null());
}

}  // namespace content