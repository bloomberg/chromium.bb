// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_version.h"

#include <stdint.h>
#include <tuple>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_service.mojom.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_event_status.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

// IPC messages for testing ---------------------------------------------------

#define IPC_MESSAGE_IMPL
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START TestMsgStart

IPC_MESSAGE_CONTROL0(TestMsg_Message)
IPC_MESSAGE_ROUTED1(TestMsg_MessageFromWorker, int)

IPC_MESSAGE_ROUTED2(TestMsg_TestEventResult, int, std::string)

// ---------------------------------------------------------------------------

namespace content {

namespace {

class MessageReceiver : public EmbeddedWorkerTestHelper {
 public:
  MessageReceiver() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~MessageReceiver() override {}

  void SimulateSendValueToBrowser(int embedded_worker_id, int value) {
    SimulateSend(new TestMsg_MessageFromWorker(embedded_worker_id, value));
  }

  void SimulateSendEventResult(int embedded_worker_id,
                               int request_id,
                               const std::string& reply) {
    SimulateSend(
        new TestMsg_TestEventResult(embedded_worker_id, request_id, reply));
  }

 private:
  void OnMessage() {
    // Do nothing.
  }

  DISALLOW_COPY_AND_ASSIGN(MessageReceiver);
};

void VerifyCalled(bool* called) {
  *called = true;
}

void ObserveStatusChanges(ServiceWorkerVersion* version,
                          std::vector<ServiceWorkerVersion::Status>* statuses) {
  statuses->push_back(version->status());
  version->RegisterStatusChangeCallback(base::BindOnce(
      &ObserveStatusChanges, base::Unretained(version), statuses));
}

// A specialized listener class to receive test messages from a worker.
class MessageReceiverFromWorker : public EmbeddedWorkerInstance::Listener {
 public:
  explicit MessageReceiverFromWorker(EmbeddedWorkerInstance* instance)
      : instance_(instance) {
    instance_->AddListener(this);
  }
  ~MessageReceiverFromWorker() override { instance_->RemoveListener(this); }

  void OnStarted() override { NOTREACHED(); }
  void OnStopped(EmbeddedWorkerStatus old_status) override { NOTREACHED(); }
  bool OnMessageReceived(const IPC::Message& message) override {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MessageReceiverFromWorker, message)
      IPC_MESSAGE_HANDLER(TestMsg_MessageFromWorker, OnMessageFromWorker)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnMessageFromWorker(int value) { received_values_.push_back(value); }
  const std::vector<int>& received_values() const { return received_values_; }

 private:
  EmbeddedWorkerInstance* instance_;
  std::vector<int> received_values_;
  DISALLOW_COPY_AND_ASSIGN(MessageReceiverFromWorker);
};

base::Time GetYesterday() {
  return base::Time::Now() - base::TimeDelta::FromDays(1) -
         base::TimeDelta::FromSeconds(1);
}

class TestServiceImpl : public mojom::TestService {
 public:
  static void Create(mojo::InterfaceRequest<mojom::TestService> request) {
    mojo::MakeStrongBinding(base::WrapUnique(new TestServiceImpl),
                            std::move(request));
  }

  void DoSomething(DoSomethingCallback callback) override {
    std::move(callback).Run();
  }

  void DoTerminateProcess(DoTerminateProcessCallback callback) override {
    NOTREACHED();
  }

  void CreateFolder(CreateFolderCallback callback) override { NOTREACHED(); }

  void GetRequestorName(GetRequestorNameCallback callback) override {
    std::move(callback).Run("");
  }

  void CreateSharedBuffer(const std::string& message,
                          CreateSharedBufferCallback callback) override {
    NOTREACHED();
  }

 private:
  TestServiceImpl() {}
};

}  // namespace

class ServiceWorkerVersionTest : public testing::Test {
 protected:
  struct RunningStateListener : public ServiceWorkerVersion::Listener {
    RunningStateListener() : last_status(EmbeddedWorkerStatus::STOPPED) {}
    ~RunningStateListener() override {}
    void OnRunningStateChanged(ServiceWorkerVersion* version) override {
      last_status = version->running_status();
    }
    EmbeddedWorkerStatus last_status;
  };

  ServiceWorkerVersionTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_ = GetMessageReceiver();

    helper_->context()->storage()->LazyInitializeForTest(
        base::BindOnce(&base::DoNothing));
    base::RunLoop().RunUntilIdle();

    pattern_ = GURL("https://www.example.com/test/");
    registration_ = new ServiceWorkerRegistration(
        blink::mojom::ServiceWorkerRegistrationOptions(pattern_), 1L,
        helper_->context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(),
        GURL("https://www.example.com/test/service_worker.js"),
        helper_->context()->storage()->NewVersionId(),
        helper_->context()->AsWeakPtr());
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(
        ServiceWorkerDatabase::ResourceRecord(10, version_->script_url(), 100));
    version_->script_cache_map()->SetResources(records);
    version_->SetMainScriptHttpResponseInfo(
        EmbeddedWorkerTestHelper::CreateHttpResponseInfo());
    version_->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);

    // Make the registration findable via storage functions.
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    helper_->context()->storage()->StoreRegistration(
        registration_.get(),
        version_.get(),
        CreateReceiverOnCurrentThread(&status));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(SERVICE_WORKER_OK, status);

    // Simulate adding one process to the pattern.
    helper_->SimulateAddProcessToPattern(pattern_,
                                         helper_->mock_render_process_id());
    ASSERT_TRUE(helper_->context()->process_manager()
        ->PatternHasProcessToRun(pattern_));
  }

  virtual std::unique_ptr<MessageReceiver> GetMessageReceiver() {
    return base::MakeUnique<MessageReceiver>();
  }

  void TearDown() override {
    version_ = 0;
    registration_ = 0;
    helper_.reset();
  }

  void SimulateDispatchEvent(ServiceWorkerMetrics::EventType event_type) {
    ServiceWorkerStatusCode status =
        SERVICE_WORKER_ERROR_MAX_VALUE;  // dummy value

    // Make sure worker is running.
    scoped_refptr<MessageLoopRunner> runner(new MessageLoopRunner);
    version_->RunAfterStartWorker(event_type, runner->QuitClosure(),
                                  CreateReceiverOnCurrentThread(&status));
    runner->Run();
    EXPECT_EQ(SERVICE_WORKER_ERROR_MAX_VALUE, status);
    EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

    // Start request, as if an event is being dispatched.
    int request_id = version_->StartRequest(
        event_type, CreateReceiverOnCurrentThread(&status));
    base::RunLoop().RunUntilIdle();

    // And finish request, as if a response to the event was received.
    EXPECT_TRUE(version_->FinishRequest(request_id, true /* was_handled */,
                                        base::Time::Now()));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(SERVICE_WORKER_ERROR_MAX_VALUE, status);
  }

  base::SimpleTestTickClock* SetTickClockForTesting() {
    base::SimpleTestTickClock* tick_clock = new base::SimpleTestTickClock();
    version_->SetTickClockForTesting(base::WrapUnique(tick_clock));
    return tick_clock;
  }

  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<MessageReceiver> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  GURL pattern_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerVersionTest);
};

class MessageReceiverDisallowStart : public MessageReceiver {
 public:
  MessageReceiverDisallowStart() : MessageReceiver() {}
  ~MessageReceiverDisallowStart() override {}

  enum class StartMode { STALL, FAIL, SUCCEED };

  void OnStartWorker(
      int embedded_worker_id,
      int64_t service_worker_version_id,
      const GURL& scope,
      const GURL& script_url,
      bool pause_after_download,
      mojom::ServiceWorkerEventDispatcherRequest request,
      mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
      mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info)
      override {
    switch (mode_) {
      case StartMode::STALL:
        // Prepare for OnStopWorker().
        instance_host_ptr_map_[embedded_worker_id].Bind(
            std::move(instance_host));
        // Just keep the connection alive.
        event_dispatcher_request_map_[embedded_worker_id] = std::move(request);
        break;
      case StartMode::FAIL:
        ASSERT_EQ(current_mock_instance_index_ + 1,
                  mock_instance_clients()->size());
        // Remove the connection by peer
        mock_instance_clients()->at(current_mock_instance_index_).reset();
        std::move(request);
        break;
      case StartMode::SUCCEED:
        MessageReceiver::OnStartWorker(
            embedded_worker_id, service_worker_version_id, scope, script_url,
            pause_after_download, std::move(request), std::move(instance_host),
            std::move(provider_info));
        break;
    }
    current_mock_instance_index_++;
  }

  void OnStopWorker(int embedded_worker_id) override {
    if (instance_host_ptr_map_[embedded_worker_id]) {
      instance_host_ptr_map_[embedded_worker_id]->OnStopped();
      base::RunLoop().RunUntilIdle();
      return;
    }
    EmbeddedWorkerTestHelper::OnStopWorker(embedded_worker_id);
  }

  void set_start_mode(StartMode mode) { mode_ = mode; }

 private:
  uint32_t current_mock_instance_index_ = 0;
  StartMode mode_ = StartMode::STALL;

  std::map<
      int /* embedded_worker_id */,
      mojom::EmbeddedWorkerInstanceHostAssociatedPtr /* instance_host_ptr */>
      instance_host_ptr_map_;
  std::map<int /* embedded_worker_id */,
           mojom::ServiceWorkerEventDispatcherRequest>
      event_dispatcher_request_map_;
  DISALLOW_COPY_AND_ASSIGN(MessageReceiverDisallowStart);
};

class ServiceWorkerFailToStartTest : public ServiceWorkerVersionTest {
 protected:
  ServiceWorkerFailToStartTest() : ServiceWorkerVersionTest() {}

  void set_start_mode(MessageReceiverDisallowStart::StartMode mode) {
    MessageReceiverDisallowStart* helper =
        static_cast<MessageReceiverDisallowStart*>(helper_.get());
    helper->set_start_mode(mode);
  }

  std::unique_ptr<MessageReceiver> GetMessageReceiver() override {
    return base::MakeUnique<MessageReceiverDisallowStart>();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFailToStartTest);
};

class NoOpStopWorkerEmbeddedWorkerInstanceClient
    : public EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient {
 public:
  explicit NoOpStopWorkerEmbeddedWorkerInstanceClient(
      base::WeakPtr<EmbeddedWorkerTestHelper> helper)
      : EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient(helper) {}
  ~NoOpStopWorkerEmbeddedWorkerInstanceClient() override {
  }

 protected:
  void StopWorker() override {
    // Do nothing.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NoOpStopWorkerEmbeddedWorkerInstanceClient);
};

class MessageReceiverDisallowStop : public MessageReceiver {
 public:
  MessageReceiverDisallowStop() : MessageReceiver() {
    CreateAndRegisterMockInstanceClient<
        NoOpStopWorkerEmbeddedWorkerInstanceClient>(AsWeakPtr());
  }
  ~MessageReceiverDisallowStop() override {}

  void OnStopWorker(int embedded_worker_id) override {
    // Do nothing.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageReceiverDisallowStop);
};

class ServiceWorkerStallInStoppingTest : public ServiceWorkerVersionTest {
 protected:
  ServiceWorkerStallInStoppingTest() : ServiceWorkerVersionTest() {}

  std::unique_ptr<MessageReceiver> GetMessageReceiver() override {
    return base::MakeUnique<MessageReceiverDisallowStop>();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerStallInStoppingTest);
};

TEST_F(ServiceWorkerVersionTest, ConcurrentStartAndStop) {
  // Call StartWorker() multiple times.
  ServiceWorkerStatusCode status1 = SERVICE_WORKER_ERROR_FAILED;
  ServiceWorkerStatusCode status2 = SERVICE_WORKER_ERROR_FAILED;
  ServiceWorkerStatusCode status3 = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status1));
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status2));

  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Call StartWorker() after it's started.
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status3));
  base::RunLoop().RunUntilIdle();

  // All should just succeed.
  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);
  EXPECT_EQ(SERVICE_WORKER_OK, status3);

  {
    // Call StopWorker() multiple times.
    bool has_stopped1 = false;
    bool has_stopped2 = false;
    version_->StopWorker(base::BindOnce(&VerifyCalled, &has_stopped1));
    version_->StopWorker(base::BindOnce(&VerifyCalled, &has_stopped2));

    EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, version_->running_status());
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());

    // All StopWorker should just succeed.
    EXPECT_TRUE(has_stopped1);
    EXPECT_TRUE(has_stopped2);
  }

  // Start worker again.
  status1 = SERVICE_WORKER_ERROR_FAILED;
  status2 = SERVICE_WORKER_ERROR_FAILED;

  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status1));

  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  {
    // Call StopWorker()
    bool has_stopped = false;
    version_->StopWorker(base::BindOnce(&VerifyCalled, &has_stopped));

    // And try calling StartWorker while StopWorker is in queue.
    version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                          CreateReceiverOnCurrentThread(&status2));

    EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, version_->running_status());
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

    // All should just succeed.
    EXPECT_EQ(SERVICE_WORKER_OK, status1);
    EXPECT_EQ(SERVICE_WORKER_OK, status2);
    EXPECT_TRUE(has_stopped);
  }
}

TEST_F(ServiceWorkerVersionTest, DispatchEventToStoppedWorker) {
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());

  // Dispatch an event without starting the worker.
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::INSTALL);

  // The worker should be now started.
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Stop the worker, and then dispatch an event immediately after that.
  bool has_stopped = false;
  version_->StopWorker(base::BindOnce(&VerifyCalled, &has_stopped));
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::INSTALL);
  EXPECT_TRUE(has_stopped);

  // The worker should be now started again.
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, StartUnregisteredButStillLiveWorker) {
  // Start the worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  // Delete the registration.
  status = SERVICE_WORKER_ERROR_FAILED;
  helper_->context()->storage()->DeleteRegistration(
      registration_->id(), registration_->pattern().GetOrigin(),
      CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(SERVICE_WORKER_OK, status);

  // The live registration is marked as deleted, but still exists.
  ASSERT_TRUE(registration_->is_deleted());

  // Stop the worker.
  bool has_stopped = false;
  version_->StopWorker(base::BindOnce(&VerifyCalled, &has_stopped));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(has_stopped);

  // Dispatch an event on the unregistered and stopped but still live worker.
  status = SERVICE_WORKER_ERROR_FAILED;
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME);

  // The worker should be now started again.
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, ReceiveMessageFromWorker) {
  // Start worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  MessageReceiverFromWorker receiver(version_->embedded_worker());

  // Simulate sending some dummy values from the worker.
  helper_->SimulateSendValueToBrowser(
      version_->embedded_worker()->embedded_worker_id(), 555);
  helper_->SimulateSendValueToBrowser(
      version_->embedded_worker()->embedded_worker_id(), 777);

  // Verify the receiver received the values.
  ASSERT_EQ(2U, receiver.received_values().size());
  EXPECT_EQ(555, receiver.received_values()[0]);
  EXPECT_EQ(777, receiver.received_values()[1]);
}

TEST_F(ServiceWorkerVersionTest, InstallAndWaitCompletion) {
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);

  // Wait for the completion.
  bool status_change_called = false;
  version_->RegisterStatusChangeCallback(
      base::BindOnce(&VerifyCalled, &status_change_called));

  // Dispatch an install event.
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::INSTALL);

  // Version's status must not have changed during installation.
  EXPECT_FALSE(status_change_called);
  EXPECT_EQ(ServiceWorkerVersion::INSTALLING, version_->status());
}

TEST_F(ServiceWorkerVersionTest, ActivateAndWaitCompletion) {
  // TODO(mek): This test (and the one above for the install event) made more
  // sense back when ServiceWorkerVersion was responsible for updating the
  // status. Now a better version of this test should probably be added to
  // ServiceWorkerRegistrationTest instead.

  version_->SetStatus(ServiceWorkerVersion::ACTIVATING);

  // Wait for the completion.
  bool status_change_called = false;
  version_->RegisterStatusChangeCallback(
      base::BindOnce(&VerifyCalled, &status_change_called));

  // Dispatch an activate event.
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::ACTIVATE);

  // Version's status must not have changed during activation.
  EXPECT_FALSE(status_change_called);
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATING, version_->status());
}

TEST_F(ServiceWorkerVersionTest, RepeatedlyObserveStatusChanges) {
  EXPECT_EQ(ServiceWorkerVersion::NEW, version_->status());

  // Repeatedly observe status changes (the callback re-registers itself).
  std::vector<ServiceWorkerVersion::Status> statuses;
  version_->RegisterStatusChangeCallback(base::BindOnce(
      &ObserveStatusChanges, base::RetainedRef(version_), &statuses));

  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  version_->SetStatus(ServiceWorkerVersion::INSTALLED);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATING);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  version_->SetStatus(ServiceWorkerVersion::REDUNDANT);

  // Verify that we could successfully observe repeated status changes.
  ASSERT_EQ(5U, statuses.size());
  ASSERT_EQ(ServiceWorkerVersion::INSTALLING, statuses[0]);
  ASSERT_EQ(ServiceWorkerVersion::INSTALLED, statuses[1]);
  ASSERT_EQ(ServiceWorkerVersion::ACTIVATING, statuses[2]);
  ASSERT_EQ(ServiceWorkerVersion::ACTIVATED, statuses[3]);
  ASSERT_EQ(ServiceWorkerVersion::REDUNDANT, statuses[4]);
}

TEST_F(ServiceWorkerVersionTest, Doom) {
  // Add a controllee.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  ServiceWorkerRemoteProviderEndpoint remote_endpoint;
  std::unique_ptr<ServiceWorkerProviderHost> host = CreateProviderHostForWindow(
      33 /* dummy render process id */, 1 /* dummy provider_id */,
      true /* is_parent_frame_secure */, helper_->context()->AsWeakPtr(),
      &remote_endpoint);
  host->SetDocumentUrl(registration_->pattern());
  host->AssociateRegistration(registration_.get(), false);
  EXPECT_TRUE(version_->HasControllee());
  EXPECT_TRUE(host->controller());

  // Doom the version.
  version_->Doom();

  // The controllee should have been removed.
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version_->status());
  EXPECT_FALSE(version_->HasControllee());
  EXPECT_FALSE(host->controller());
}

TEST_F(ServiceWorkerVersionTest, IdleTimeout) {
  // Used to reliably test when the idle time gets reset regardless of clock
  // granularity.
  const base::TimeDelta kOneSecond = base::TimeDelta::FromSeconds(1);

  // Verify the timer is not running when version initializes its status.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  EXPECT_FALSE(version_->timeout_timer_.IsRunning());

  // Verify the timer is running after the worker is started.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_FALSE(version_->idle_time_.is_null());

  // The idle time should be reset if the worker is restarted without
  // controllee.
  bool has_stopped = false;
  version_->idle_time_ -= kOneSecond;
  base::TimeTicks idle_time = version_->idle_time_;
  version_->StopWorker(base::BindOnce(&VerifyCalled, &has_stopped));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(has_stopped);
  status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_LT(idle_time, version_->idle_time_);

  // Adding a controllee resets the idle time.
  version_->idle_time_ -= kOneSecond;
  idle_time = version_->idle_time_;
  ServiceWorkerRemoteProviderEndpoint remote_endpoint;
  std::unique_ptr<ServiceWorkerProviderHost> host = CreateProviderHostForWindow(
      33 /* dummy render process id */, 1 /* dummy provider_id */,
      true /* is_parent_frame_secure */, helper_->context()->AsWeakPtr(),
      &remote_endpoint);
  version_->AddControllee(host.get());
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_LT(idle_time, version_->idle_time_);

  // Completing an event resets the idle time.
  version_->idle_time_ -= kOneSecond;
  idle_time = version_->idle_time_;
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME);
  EXPECT_LT(idle_time, version_->idle_time_);

  // Starting and finishing a request resets the idle time.
  version_->idle_time_ -= kOneSecond;
  idle_time = version_->idle_time_;
  int request_id =
      version_->StartRequest(ServiceWorkerMetrics::EventType::SYNC,
                             CreateReceiverOnCurrentThread(&status));
  EXPECT_TRUE(version_->FinishRequest(request_id, true /* was_handled */,
                                      base::Time::Now()));

  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_LT(idle_time, version_->idle_time_);
}

TEST_F(ServiceWorkerVersionTest, SetDevToolsAttached) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));

  ASSERT_EQ(EmbeddedWorkerStatus::STARTING, version_->running_status());

  ASSERT_TRUE(version_->timeout_timer_.IsRunning());
  ASSERT_FALSE(version_->start_time_.is_null());
  ASSERT_FALSE(version_->skip_recording_startup_time_);

  // Simulate DevTools is attached. This should deactivate the timer for start
  // timeout, but not stop the timer itself.
  version_->SetDevToolsAttached(true);
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_TRUE(version_->start_time_.is_null());
  EXPECT_TRUE(version_->skip_recording_startup_time_);

  // Simulate DevTools is detached. This should reactivate the timer for start
  // timeout.
  version_->SetDevToolsAttached(false);
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_FALSE(version_->start_time_.is_null());
  EXPECT_TRUE(version_->skip_recording_startup_time_);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, StoppingBeforeDestruct) {
  RunningStateListener listener;
  version_->AddListener(&listener);

  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, listener.last_status);

  // Destruct |version_| by releasing all references, including the provider
  // host's.
  helper_->context()->RemoveProviderHost(
      version_->provider_host()->process_id(),
      version_->provider_host()->provider_id());
  version_ = nullptr;
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, listener.last_status);
}

// Test that update isn't triggered for a non-stale worker.
TEST_F(ServiceWorkerVersionTest, StaleUpdate_FreshWorker) {
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_last_update_check(base::Time::Now());
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);

  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_FALSE(version_->update_timer_.IsRunning());
}

// Test that update isn't triggered for a non-active worker.
TEST_F(ServiceWorkerVersionTest, StaleUpdate_NonActiveWorker) {
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  registration_->SetInstallingVersion(version_);
  registration_->set_last_update_check(GetYesterday());
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::INSTALL);

  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_FALSE(version_->update_timer_.IsRunning());
}

// Test that staleness is detected when starting a worker.
TEST_F(ServiceWorkerVersionTest, StaleUpdate_StartWorker) {
  // Starting the worker marks it as stale.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_last_update_check(GetYesterday());
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);
  EXPECT_FALSE(version_->stale_time_.is_null());
  EXPECT_FALSE(version_->update_timer_.IsRunning());

  // Update is actually scheduled after the worker stops.
  bool has_stopped = false;
  version_->StopWorker(base::BindOnce(&VerifyCalled, &has_stopped));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(has_stopped);
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_TRUE(version_->update_timer_.IsRunning());
}

// Test that staleness is detected on a running worker.
TEST_F(ServiceWorkerVersionTest, StaleUpdate_RunningWorker) {
  // Start a fresh worker.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_last_update_check(base::Time::Now());
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);
  EXPECT_TRUE(version_->stale_time_.is_null());

  // Simulate it running for a day. It will be marked stale.
  registration_->set_last_update_check(GetYesterday());
  version_->OnTimeoutTimer();
  EXPECT_FALSE(version_->stale_time_.is_null());
  EXPECT_FALSE(version_->update_timer_.IsRunning());

  // Simulate it running for past the wait threshold. The update will be
  // scheduled.
  version_->stale_time_ = base::TimeTicks::Now() -
                          ServiceWorkerVersion::kStartNewWorkerTimeout -
                          base::TimeDelta::FromMinutes(1);
  version_->OnTimeoutTimer();
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_TRUE(version_->update_timer_.IsRunning());
}

// Test that a stream of events doesn't restart the timer.
TEST_F(ServiceWorkerVersionTest, StaleUpdate_DoNotDeferTimer) {
  // Make a stale worker.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_last_update_check(GetYesterday());
  base::TimeTicks stale_time = base::TimeTicks::Now() -
                               ServiceWorkerVersion::kStartNewWorkerTimeout -
                               base::TimeDelta::FromMinutes(1);
  version_->stale_time_ = stale_time;

  // Stale time is not deferred.
  version_->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::UNKNOWN,
      base::BindOnce(&base::DoNothing),
      base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback));
  version_->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::UNKNOWN,
      base::BindOnce(&base::DoNothing),
      base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(stale_time, version_->stale_time_);

  // Timeout triggers the update.
  version_->OnTimeoutTimer();
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_TRUE(version_->update_timer_.IsRunning());

  // Update timer is not deferred.
  base::TimeTicks run_time = version_->update_timer_.desired_run_time();
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_EQ(run_time, version_->update_timer_.desired_run_time());
}

class MessageReceiverControlEvents : public MessageReceiver {
 public:
  MessageReceiverControlEvents() : MessageReceiver() {}
  ~MessageReceiverControlEvents() override {}

  void OnExtendableMessageEvent(
      mojom::ExtendableMessageEventPtr event,
      mojom::ServiceWorkerEventDispatcher::
          DispatchExtendableMessageEventCallback callback) override {
    EXPECT_FALSE(extendable_message_event_callback_);
    extendable_message_event_callback_ = std::move(callback);
  }

  void OnStopWorker(int embedded_worker_id) override {
    EXPECT_FALSE(stop_worker_callback_);
    stop_worker_callback_ =
        base::BindOnce(&MessageReceiverControlEvents::SimulateWorkerStopped,
                       base::Unretained(this), embedded_worker_id);
  }

  bool has_extendable_message_event_callback() {
    return !extendable_message_event_callback_.is_null();
  }

  mojom::ServiceWorkerEventDispatcher::DispatchExtendableMessageEventCallback
  TakeExtendableMessageEventCallback() {
    return std::move(extendable_message_event_callback_);
  }

  base::OnceClosure stop_worker_callback() {
    return std::move(stop_worker_callback_);
  }

 private:
  mojom::ServiceWorkerEventDispatcher::DispatchExtendableMessageEventCallback
      extendable_message_event_callback_;
  base::OnceClosure stop_worker_callback_;
};

class ServiceWorkerRequestTimeoutTest : public ServiceWorkerVersionTest {
 protected:
  ServiceWorkerRequestTimeoutTest() : ServiceWorkerVersionTest() {}

  std::unique_ptr<MessageReceiver> GetMessageReceiver() override {
    return base::MakeUnique<MessageReceiverControlEvents>();
  }

  bool has_extendable_message_event_callback() {
    return static_cast<MessageReceiverControlEvents*>(helper_.get())
        ->has_extendable_message_event_callback();
  }

  mojom::ServiceWorkerEventDispatcher::DispatchExtendableMessageEventCallback
  TakeExtendableMessageEventCallback() {
    return static_cast<MessageReceiverControlEvents*>(helper_.get())
        ->TakeExtendableMessageEventCallback();
  }

  base::OnceClosure stop_worker_callback() {
    return static_cast<MessageReceiverControlEvents*>(helper_.get())
        ->stop_worker_callback();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRequestTimeoutTest);
};

TEST_F(ServiceWorkerRequestTimeoutTest, RequestTimeout) {
  ServiceWorkerStatusCode error_status =
      SERVICE_WORKER_ERROR_NETWORK;  // dummy value
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  version_->StartWorker(
      ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME,
      base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();

  // Create a request.
  int request_id =
      version_->StartRequest(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME,
                             CreateReceiverOnCurrentThread(&error_status));

  // Dispatch a dummy event whose response will be received by SWVersion.
  EXPECT_FALSE(has_extendable_message_event_callback());
  version_->event_dispatcher()->DispatchExtendableMessageEvent(
      mojom::ExtendableMessageEvent::New(),
      version_->CreateSimpleEventCallback(request_id));

  base::RunLoop().RunUntilIdle();
  // The renderer should have received an ExtendableMessageEvent request.
  EXPECT_TRUE(has_extendable_message_event_callback());

  // Callback has not completed yet.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NETWORK, error_status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Simulate timeout.
  EXPECT_FALSE(stop_worker_callback());
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->SetAllRequestExpirations(base::TimeTicks::Now());
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();

  base::OnceClosure callback = stop_worker_callback();

  // The renderer should have received a StopWorker request.
  EXPECT_TRUE(callback);
  // The request should have timed out.
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, error_status);
  // Calling FinishRequest should be no-op, since the request timed out.
  EXPECT_FALSE(version_->FinishRequest(request_id, true /* was_handled */,
                                       base::Time::Now()));

  // Simulate the renderer aborting the pending event.
  // This should not crash: https://crbug.com/676984.
  TakeExtendableMessageEventCallback().Run(
      blink::mojom::ServiceWorkerEventStatus::ABORTED, base::Time::Now());
  base::RunLoop().RunUntilIdle();

  // Simulate the renderer stopping the worker.
  std::move(callback).Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, RequestNowTimeout) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  version_->StartWorker(
      ServiceWorkerMetrics::EventType::SYNC,
      base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();

  // Create a request that should expire Now().
  int request_id = version_->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC,
      CreateReceiverOnCurrentThread(&status), base::TimeDelta(),
      ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, status);

  EXPECT_FALSE(version_->FinishRequest(request_id, true /* was_handled */,
                                       base::Time::Now()));

  // CONTINUE_ON_TIMEOUT timeouts don't stop the service worker.
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, RequestNowTimeoutKill) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  version_->StartWorker(
      ServiceWorkerMetrics::EventType::SYNC,
      base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();

  // Create a request that should expire Now().
  int request_id = version_->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC,
      CreateReceiverOnCurrentThread(&status), base::TimeDelta(),
      ServiceWorkerVersion::KILL_ON_TIMEOUT);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, status);

  EXPECT_FALSE(version_->FinishRequest(request_id, true /* was_handled */,
                                       base::Time::Now()));

  // KILL_ON_TIMEOUT timeouts should stop the service worker.
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, RequestCustomizedTimeout) {
  ServiceWorkerStatusCode first_status =
      SERVICE_WORKER_ERROR_MAX_VALUE;  // dummy value
  ServiceWorkerStatusCode second_status =
      SERVICE_WORKER_ERROR_MAX_VALUE;  // dummy value
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  version_->StartWorker(
      ServiceWorkerMetrics::EventType::SYNC,
      base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();

  base::SimpleTestTickClock* tick_clock = SetTickClockForTesting();

  // Create two requests. One which times out in 10 seconds, one in 20 seconds.
  int timeout_seconds = 10;
  int first_request_id = version_->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC,
      CreateReceiverOnCurrentThread(&first_status),
      base::TimeDelta::FromSeconds(2 * timeout_seconds),
      ServiceWorkerVersion::KILL_ON_TIMEOUT);

  int second_request_id = version_->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC,
      CreateReceiverOnCurrentThread(&second_status),
      base::TimeDelta::FromSeconds(timeout_seconds),
      ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);

  // The status should not have changed since neither task has timed out yet.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_MAX_VALUE, first_status);
  EXPECT_EQ(SERVICE_WORKER_ERROR_MAX_VALUE, second_status);

  // Now advance time until the second task timeout should expire.
  tick_clock->Advance(base::TimeDelta::FromSeconds(timeout_seconds + 1));
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_MAX_VALUE, first_status);
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, second_status);

  // CONTINUE_ON_TIMEOUT timeouts don't stop the service worker.
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Now advance time until both tasks should be expired.
  tick_clock->Advance(base::TimeDelta::FromSeconds(timeout_seconds + 1));
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, first_status);
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, second_status);

  EXPECT_FALSE(version_->FinishRequest(first_request_id, true /* was_handled */,
                                       base::Time::Now()));

  EXPECT_FALSE(version_->FinishRequest(
      second_request_id, true /* was_handled */, base::Time::Now()));

  // KILL_ON_TIMEOUT timeouts should stop the service worker.
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, MixedRequestTimeouts) {
  ServiceWorkerStatusCode sync_status =
      SERVICE_WORKER_ERROR_NETWORK;  // dummy value
  ServiceWorkerStatusCode fetch_status =
      SERVICE_WORKER_ERROR_NETWORK;  // dummy value
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  version_->StartWorker(
      ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME,
      base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();

  // Create a fetch request that should expire sometime later.
  int fetch_request_id =
      version_->StartRequest(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME,
                             CreateReceiverOnCurrentThread(&fetch_status));
  // Create a request that should expire Now().
  int sync_request_id = version_->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC,
      CreateReceiverOnCurrentThread(&sync_status), base::TimeDelta(),
      ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_NETWORK, sync_status);

  // Verify the sync has timed out but not the fetch.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, sync_status);
  EXPECT_EQ(SERVICE_WORKER_ERROR_NETWORK, fetch_status);

  // Background sync timeouts don't stop the service worker.
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Gracefully handle the sync event finishing after the timeout.
  EXPECT_FALSE(version_->FinishRequest(sync_request_id, true /* was_handled */,
                                       base::Time::Now()));

  // Verify that the fetch times out later.
  version_->SetAllRequestExpirations(base::TimeTicks::Now());
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, fetch_status);

  // Fetch request should no longer exist.
  EXPECT_FALSE(version_->FinishRequest(fetch_request_id, true /* was_handled */,
                                       base::Time::Now()));

  // Other timeouts do stop the service worker.
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());
}

TEST_F(ServiceWorkerFailToStartTest, RendererCrash) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();

  // Callback has not completed yet.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NETWORK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, version_->running_status());

  // Simulate renderer crash: break EmbeddedWorkerInstance's Mojo connection to
  // the renderer-side client.
  helper_->mock_instance_clients()->clear();
  base::RunLoop().RunUntilIdle();

  // Callback completed.
  EXPECT_EQ(SERVICE_WORKER_ERROR_START_WORKER_FAILED, status);
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());
}

TEST_F(ServiceWorkerFailToStartTest, Timeout) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value

  // Start starting the worker.
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();

  // Callback has not completed yet.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NETWORK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, version_->running_status());

  // Simulate timeout.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->start_time_ = base::TimeTicks::Now() -
                          ServiceWorkerVersion::kStartNewWorkerTimeout -
                          base::TimeDelta::FromMinutes(1);
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, status);
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());
}

// Test that a service worker stalled in stopping will timeout and not get in a
// sticky error state.
TEST_F(ServiceWorkerStallInStoppingTest, DetachThenStart) {
  // Start a worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Try to stop the worker.
  bool has_stopped = false;
  version_->StopWorker(base::BindOnce(&VerifyCalled, &has_stopped));
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, version_->running_status());
  base::RunLoop().RunUntilIdle();

  // Worker is now stalled in stopping. Verify a fast timeout is in place.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_EQ(ServiceWorkerVersion::kStopWorkerTimeout,
            version_->timeout_timer_.GetCurrentDelay());

  // Simulate timeout.
  version_->stop_time_ = base::TimeTicks::Now() -
                         ServiceWorkerVersion::kStopWorkerTimeout -
                         base::TimeDelta::FromSeconds(1);
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(has_stopped);
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());

  // Try to start the worker again. It should work.
  status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // The timeout interval should be reset to normal.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_EQ(ServiceWorkerVersion::kTimeoutTimerDelay,
            version_->timeout_timer_.GetCurrentDelay());
}

// Test that a service worker stalled in stopping with a start worker
// request queued up will timeout and restart.
TEST_F(ServiceWorkerStallInStoppingTest, DetachThenRestart) {
  // Start a worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Try to stop the worker.
  bool has_stopped = false;
  version_->StopWorker(base::BindOnce(&VerifyCalled, &has_stopped));
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, version_->running_status());

  // Worker is now stalled in stopping. Add a start worker request.
  ServiceWorkerStatusCode start_status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&start_status));

  // Simulate timeout. The worker should stop and get restarted.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->stop_time_ = base::TimeTicks::Now() -
                         ServiceWorkerVersion::kStopWorkerTimeout -
                         base::TimeDelta::FromSeconds(1);
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(has_stopped);
  EXPECT_EQ(SERVICE_WORKER_OK, start_status);
}

TEST_F(ServiceWorkerVersionTest, RegisterForeignFetchScopes) {
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  // Start a worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());
  EXPECT_EQ(0, helper_->mock_render_process_host()->bad_msg_count());

  GURL valid_scope_1("https://www.example.com/test/subscope");
  GURL valid_scope_2("https://www.example.com/test/othersubscope");
  std::vector<GURL> valid_scopes;
  valid_scopes.push_back(valid_scope_1);
  valid_scopes.push_back(valid_scope_2);

  std::vector<url::Origin> all_origins;
  url::Origin valid_origin(GURL("https://chromium.org/"));
  std::vector<url::Origin> valid_origin_list(1, valid_origin);

  // Invalid subscope, should kill worker (but in tests will only increase bad
  // message count).
  version_->RegisterForeignFetchScopes(std::vector<GURL>(1, GURL()),
                                       valid_origin_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, helper_->mock_render_process_host()->bad_msg_count());
  EXPECT_EQ(0u, version_->foreign_fetch_scopes_.size());
  EXPECT_EQ(0u, version_->foreign_fetch_origins_.size());

  // Subscope outside the scope of the worker.
  version_->RegisterForeignFetchScopes(
      std::vector<GURL>(1, GURL("https://www.example.com/wrong")),
      valid_origin_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, helper_->mock_render_process_host()->bad_msg_count());
  EXPECT_EQ(0u, version_->foreign_fetch_scopes_.size());
  EXPECT_EQ(0u, version_->foreign_fetch_origins_.size());

  // Subscope on wrong origin.
  version_->RegisterForeignFetchScopes(
      std::vector<GURL>(1, GURL("https://example.com/test/")),
      valid_origin_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, helper_->mock_render_process_host()->bad_msg_count());
  EXPECT_EQ(0u, version_->foreign_fetch_scopes_.size());
  EXPECT_EQ(0u, version_->foreign_fetch_origins_.size());

  // Invalid origin.
  version_->RegisterForeignFetchScopes(
      valid_scopes, std::vector<url::Origin>(1, url::Origin()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, helper_->mock_render_process_host()->bad_msg_count());
  EXPECT_EQ(0u, version_->foreign_fetch_scopes_.size());
  EXPECT_EQ(0u, version_->foreign_fetch_origins_.size());

  // Valid subscope, no origins.
  version_->RegisterForeignFetchScopes(std::vector<GURL>(1, valid_scope_1),
                                       all_origins);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, helper_->mock_render_process_host()->bad_msg_count());
  EXPECT_EQ(1u, version_->foreign_fetch_scopes_.size());
  EXPECT_EQ(valid_scope_1, version_->foreign_fetch_scopes_[0]);
  EXPECT_EQ(0u, version_->foreign_fetch_origins_.size());

  // Valid subscope, explicit origins.
  version_->RegisterForeignFetchScopes(valid_scopes, valid_origin_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, helper_->mock_render_process_host()->bad_msg_count());
  EXPECT_EQ(2u, version_->foreign_fetch_scopes_.size());
  EXPECT_EQ(valid_scope_1, version_->foreign_fetch_scopes_[0]);
  EXPECT_EQ(valid_scope_2, version_->foreign_fetch_scopes_[1]);
  EXPECT_EQ(1u, version_->foreign_fetch_origins_.size());
  EXPECT_EQ(valid_origin, version_->foreign_fetch_origins_[0]);
}

TEST_F(ServiceWorkerVersionTest, RendererCrashDuringEvent) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  version_->StartWorker(ServiceWorkerMetrics::EventType::SYNC,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  int request_id =
      version_->StartRequest(ServiceWorkerMetrics::EventType::SYNC,
                             CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();

  // Callback has not completed yet.
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  // Simulate renderer crash: break EmbeddedWorkerInstance's Mojo connection to
  // the renderer-side client.
  helper_->mock_instance_clients()->clear();
  base::RunLoop().RunUntilIdle();

  // Callback completed.
  EXPECT_EQ(SERVICE_WORKER_ERROR_FAILED, status);
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());

  // Request already failed, calling finsh should return false.
  EXPECT_FALSE(version_->FinishRequest(request_id, true /* was_handled */,
                                       base::Time::Now()));
}

TEST_F(ServiceWorkerFailToStartTest, FailingWorkerUsesNewRendererProcess) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;

  helper_->SimulateAddProcessToPattern(pattern_,
                                       helper_->new_render_process_id());
  ServiceWorkerContextCore* context = helper_->context();
  int64_t id = version_->version_id();
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  // Start once. It should choose the "existing process".
  set_start_mode(MessageReceiverDisallowStart::StartMode::SUCCEED);
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(helper_->mock_render_process_id(),
            version_->embedded_worker()->process_id());
  version_->StopWorker(base::BindOnce(&base::DoNothing));
  base::RunLoop().RunUntilIdle();

  // Fail once.
  set_start_mode(MessageReceiverDisallowStart::StartMode::FAIL);
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_START_WORKER_FAILED, status);
  EXPECT_EQ(1, context->GetVersionFailureCount(id));

  // Fail again.
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_START_WORKER_FAILED, status);
  EXPECT_EQ(2, context->GetVersionFailureCount(id));

  // Succeed. It should choose the "new process".
  set_start_mode(MessageReceiverDisallowStart::StartMode::SUCCEED);
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(helper_->new_render_process_id(),
            version_->embedded_worker()->process_id());
  EXPECT_EQ(0, context->GetVersionFailureCount(id));
  version_->StopWorker(base::BindOnce(&base::DoNothing));
  base::RunLoop().RunUntilIdle();

  // Start again. It should choose the "existing process" again as we no longer
  // force creation of a new process.
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(helper_->mock_render_process_id(),
            version_->embedded_worker()->process_id());
  version_->StopWorker(base::BindOnce(&base::DoNothing));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ServiceWorkerFailToStartTest, RestartStalledWorker) {
  ServiceWorkerStatusCode status1 = SERVICE_WORKER_ERROR_MAX_VALUE;
  version_->StartWorker(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME,
                        CreateReceiverOnCurrentThread(&status1));
  base::RunLoop().RunUntilIdle();
  // The default start mode is StartMode::STALL. So the callback of StartWorker
  // is not called yet.
  EXPECT_EQ(SERVICE_WORKER_ERROR_MAX_VALUE, status1);

  // Set StartMode::SUCCEED. So the next start worker will be successful.
  set_start_mode(MessageReceiverDisallowStart::StartMode::SUCCEED);

  // StartWorker message will be sent again because OnStopped is called before
  // OnStarted.
  bool has_stopped = false;
  version_->StopWorker(base::BindOnce(&VerifyCalled, &has_stopped));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_TRUE(has_stopped);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());
}

class ServiceWorkerNavigationHintUMATest : public ServiceWorkerVersionTest {
 protected:
  ServiceWorkerNavigationHintUMATest() : ServiceWorkerVersionTest() {}

  void StartWorker(ServiceWorkerMetrics::EventType purpose) {
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
    version_->StartWorker(purpose, CreateReceiverOnCurrentThread(&status));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(SERVICE_WORKER_OK, status);
  }

  void StopWorker() {
    bool has_stopped = false;
    version_->StopWorker(base::BindOnce(&VerifyCalled, &has_stopped));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(has_stopped);
  }

  static const char kStartHintPrecision[];

  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerNavigationHintUMATest);
};

const char ServiceWorkerNavigationHintUMATest::kStartHintPrecision[] =
    "ServiceWorker.StartHintPrecision";

TEST_F(ServiceWorkerNavigationHintUMATest, Precision) {
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  StartWorker(ServiceWorkerMetrics::EventType::NAVIGATION_HINT);
  StopWorker();
  histogram_tester_.ExpectBucketCount(kStartHintPrecision, true, 0);
  histogram_tester_.ExpectBucketCount(kStartHintPrecision, false, 1);

  StartWorker(ServiceWorkerMetrics::EventType::NAVIGATION_HINT);
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::MESSAGE);
  StopWorker();
  histogram_tester_.ExpectBucketCount(kStartHintPrecision, true, 0);
  histogram_tester_.ExpectBucketCount(kStartHintPrecision, false, 2);

  StartWorker(ServiceWorkerMetrics::EventType::NAVIGATION_HINT);
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME);
  StopWorker();
  histogram_tester_.ExpectBucketCount(kStartHintPrecision, true, 1);
  histogram_tester_.ExpectBucketCount(kStartHintPrecision, false, 2);

  StartWorker(ServiceWorkerMetrics::EventType::NAVIGATION_HINT);
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::FETCH_SUB_FRAME);
  StopWorker();
  histogram_tester_.ExpectBucketCount(kStartHintPrecision, true, 2);
  histogram_tester_.ExpectBucketCount(kStartHintPrecision, false, 2);
}

TEST_F(ServiceWorkerNavigationHintUMATest, ConcurrentStart) {
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  ServiceWorkerStatusCode status1 = SERVICE_WORKER_ERROR_MAX_VALUE;
  ServiceWorkerStatusCode status2 = SERVICE_WORKER_ERROR_MAX_VALUE;
  version_->StartWorker(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME,
                        CreateReceiverOnCurrentThread(&status1));
  version_->StartWorker(ServiceWorkerMetrics::EventType::NAVIGATION_HINT,
                        CreateReceiverOnCurrentThread(&status2));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);
  StopWorker();
  // The first purpose of starting worker was not a navigation hint.
  histogram_tester_.ExpectTotalCount(kStartHintPrecision, 0);

  status1 = SERVICE_WORKER_ERROR_MAX_VALUE;
  status2 = SERVICE_WORKER_ERROR_MAX_VALUE;
  version_->StartWorker(ServiceWorkerMetrics::EventType::NAVIGATION_HINT,
                        CreateReceiverOnCurrentThread(&status2));
  version_->StartWorker(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME,
                        CreateReceiverOnCurrentThread(&status1));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME);
  StopWorker();
  // The first purpose of starting worker was a navigation hint.
  histogram_tester_.ExpectTotalCount(kStartHintPrecision, 1);
  histogram_tester_.ExpectBucketCount(kStartHintPrecision, true, 1);
  histogram_tester_.ExpectBucketCount(kStartHintPrecision, false, 0);
}

TEST_F(ServiceWorkerNavigationHintUMATest, StartWhileStopping) {
  StartWorker(ServiceWorkerMetrics::EventType::NAVIGATION_HINT);
  bool has_stopped = false;
  version_->StopWorker(base::BindOnce(&VerifyCalled, &has_stopped));
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, version_->running_status());
  histogram_tester_.ExpectTotalCount(kStartHintPrecision, 0);

  StartWorker(ServiceWorkerMetrics::EventType::NAVIGATION_HINT);
  // The UMA must be recorded while restarting.
  histogram_tester_.ExpectTotalCount(kStartHintPrecision, 1);
  EXPECT_TRUE(has_stopped);
  StopWorker();
  // The UMA must be recorded when the worker stopped.
  histogram_tester_.ExpectTotalCount(kStartHintPrecision, 2);
}

}  // namespace content
