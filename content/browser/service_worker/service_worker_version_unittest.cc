// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

// IPC messages for testing ---------------------------------------------------

#define IPC_MESSAGE_IMPL
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START TestMsgStart

IPC_MESSAGE_CONTROL0(TestMsg_Message);
IPC_MESSAGE_ROUTED1(TestMsg_MessageFromWorker, int);

// ---------------------------------------------------------------------------

namespace content {

namespace {

static const int kRenderProcessId = 1;

class MessageReceiver : public EmbeddedWorkerTestHelper {
 public:
  MessageReceiver()
      : EmbeddedWorkerTestHelper(kRenderProcessId),
        current_embedded_worker_id_(0) {}
  ~MessageReceiver() override {}

  bool OnMessageToWorker(int thread_id,
                         int embedded_worker_id,
                         const IPC::Message& message) override {
    if (EmbeddedWorkerTestHelper::OnMessageToWorker(
            thread_id, embedded_worker_id, message)) {
      return true;
    }
    current_embedded_worker_id_ = embedded_worker_id;
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MessageReceiver, message)
      IPC_MESSAGE_HANDLER(TestMsg_Message, OnMessage)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void SimulateSendValueToBrowser(int embedded_worker_id, int value) {
    SimulateSend(new TestMsg_MessageFromWorker(embedded_worker_id, value));
  }

 private:
  void OnMessage() {
    // Do nothing.
  }

  int current_embedded_worker_id_;
  DISALLOW_COPY_AND_ASSIGN(MessageReceiver);
};

void VerifyCalled(bool* called) {
  *called = true;
}

void ObserveStatusChanges(ServiceWorkerVersion* version,
                          std::vector<ServiceWorkerVersion::Status>* statuses) {
  statuses->push_back(version->status());
  version->RegisterStatusChangeCallback(
      base::Bind(&ObserveStatusChanges, base::Unretained(version), statuses));
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
  void OnStopped(EmbeddedWorkerInstance::Status old_status) override {
    NOTREACHED();
  }
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

}  // namespace

class ServiceWorkerVersionTest : public testing::Test {
 protected:
  ServiceWorkerVersionTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_ = GetMessageReceiver();

    pattern_ = GURL("http://www.example.com/");
    registration_ = new ServiceWorkerRegistration(
        pattern_,
        1L,
        helper_->context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(),
        GURL("http://www.example.com/service_worker.js"),
        1L,
        helper_->context()->AsWeakPtr());

    // Simulate adding one process to the pattern.
    helper_->SimulateAddProcessToPattern(pattern_, kRenderProcessId);
    ASSERT_TRUE(helper_->context()->process_manager()
        ->PatternHasProcessToRun(pattern_));
  }

  virtual scoped_ptr<MessageReceiver> GetMessageReceiver() {
    return make_scoped_ptr(new MessageReceiver());
  }

  void TearDown() override {
    version_ = 0;
    registration_ = 0;
    helper_.reset();
  }

  TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<MessageReceiver> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  GURL pattern_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerVersionTest);
};

class MessageReceiverDisallowStart : public MessageReceiver {
 public:
  MessageReceiverDisallowStart()
      : MessageReceiver() {}
  ~MessageReceiverDisallowStart() override {}

  void OnStartWorker(int embedded_worker_id,
                     int64 service_worker_version_id,
                     const GURL& scope,
                     const GURL& script_url,
                     bool pause_after_download) override {
    // Do nothing.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageReceiverDisallowStart);
};

class ServiceWorkerFailToStartTest : public ServiceWorkerVersionTest {
 protected:
  ServiceWorkerFailToStartTest()
      : ServiceWorkerVersionTest() {}

  scoped_ptr<MessageReceiver> GetMessageReceiver() override {
    return make_scoped_ptr(new MessageReceiverDisallowStart());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFailToStartTest);
};

TEST_F(ServiceWorkerVersionTest, ConcurrentStartAndStop) {
  // Call StartWorker() multiple times.
  ServiceWorkerStatusCode status1 = SERVICE_WORKER_ERROR_FAILED;
  ServiceWorkerStatusCode status2 = SERVICE_WORKER_ERROR_FAILED;
  ServiceWorkerStatusCode status3 = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(CreateReceiverOnCurrentThread(&status1));
  version_->StartWorker(CreateReceiverOnCurrentThread(&status2));

  EXPECT_EQ(ServiceWorkerVersion::STARTING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());

  // Call StartWorker() after it's started.
  version_->StartWorker(CreateReceiverOnCurrentThread(&status3));
  base::RunLoop().RunUntilIdle();

  // All should just succeed.
  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);
  EXPECT_EQ(SERVICE_WORKER_OK, status3);

  // Call StopWorker() multiple times.
  status1 = SERVICE_WORKER_ERROR_FAILED;
  status2 = SERVICE_WORKER_ERROR_FAILED;
  version_->StopWorker(CreateReceiverOnCurrentThread(&status1));
  version_->StopWorker(CreateReceiverOnCurrentThread(&status2));

  EXPECT_EQ(ServiceWorkerVersion::STOPPING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ServiceWorkerVersion::STOPPED, version_->running_status());

  // All StopWorker should just succeed.
  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);

  // Start worker again.
  status1 = SERVICE_WORKER_ERROR_FAILED;
  status2 = SERVICE_WORKER_ERROR_FAILED;
  status3 = SERVICE_WORKER_ERROR_FAILED;

  version_->StartWorker(CreateReceiverOnCurrentThread(&status1));

  EXPECT_EQ(ServiceWorkerVersion::STARTING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());

  // Call StopWorker()
  status2 = SERVICE_WORKER_ERROR_FAILED;
  version_->StopWorker(CreateReceiverOnCurrentThread(&status2));

  // And try calling StartWorker while StopWorker is in queue.
  version_->StartWorker(CreateReceiverOnCurrentThread(&status3));

  EXPECT_EQ(ServiceWorkerVersion::STOPPING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());

  // All should just succeed.
  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);
  EXPECT_EQ(SERVICE_WORKER_OK, status3);
}

TEST_F(ServiceWorkerVersionTest, DispatchEventToStoppedWorker) {
  EXPECT_EQ(ServiceWorkerVersion::STOPPED, version_->running_status());

  // Dispatch an event without starting the worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  version_->DispatchInstallEvent(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  // The worker should be now started.
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());

  // Stop the worker, and then dispatch an event immediately after that.
  status = SERVICE_WORKER_ERROR_FAILED;
  ServiceWorkerStatusCode stop_status = SERVICE_WORKER_ERROR_FAILED;
  version_->StopWorker(CreateReceiverOnCurrentThread(&stop_status));
  version_->DispatchInstallEvent(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, stop_status);

  // Dispatch an event should return SERVICE_WORKER_OK since the worker
  // should have been restarted to dispatch the event.
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  // The worker should be now started again.
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, ReceiveMessageFromWorker) {
  // Start worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(CreateReceiverOnCurrentThread(&status));
  EXPECT_EQ(ServiceWorkerVersion::STARTING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());

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

  // Dispatch an install event.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->DispatchInstallEvent(CreateReceiverOnCurrentThread(&status));

  // Wait for the completion.
  bool status_change_called = false;
  version_->RegisterStatusChangeCallback(
      base::Bind(&VerifyCalled, &status_change_called));

  base::RunLoop().RunUntilIdle();

  // Version's status must not have changed during installation.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_FALSE(status_change_called);
  EXPECT_EQ(ServiceWorkerVersion::INSTALLING, version_->status());
}

TEST_F(ServiceWorkerVersionTest, ActivateAndWaitCompletion) {
  version_->SetStatus(ServiceWorkerVersion::ACTIVATING);

  // Dispatch an activate event.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->DispatchActivateEvent(CreateReceiverOnCurrentThread(&status));

  // Wait for the completion.
  bool status_change_called = false;
  version_->RegisterStatusChangeCallback(
      base::Bind(&VerifyCalled, &status_change_called));

  base::RunLoop().RunUntilIdle();

  // Version's status must not have changed during activation.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_FALSE(status_change_called);
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATING, version_->status());
}

TEST_F(ServiceWorkerVersionTest, RepeatedlyObserveStatusChanges) {
  EXPECT_EQ(ServiceWorkerVersion::NEW, version_->status());

  // Repeatedly observe status changes (the callback re-registers itself).
  std::vector<ServiceWorkerVersion::Status> statuses;
  version_->RegisterStatusChangeCallback(
      base::Bind(&ObserveStatusChanges, version_, &statuses));

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

TEST_F(ServiceWorkerVersionTest, ScheduleStopWorker) {
  // Verify the timer is not running when version initializes its status.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  EXPECT_FALSE(version_->stop_worker_timer_.IsRunning());

  // Verify the timer is running after the worker is started.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(version_->stop_worker_timer_.IsRunning());

  // The timer should be running if the worker is restarted without controllee.
  status = SERVICE_WORKER_ERROR_FAILED;
  version_->StopWorker(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(version_->stop_worker_timer_.IsRunning());

  // Adding controllee doesn't stop the stop-worker-timer.
  scoped_ptr<ServiceWorkerProviderHost> host(
      new ServiceWorkerProviderHost(33 /* dummy render process id */,
                                    MSG_ROUTING_NONE /* render_frame_id */,
                                    1 /* dummy provider_id */,
                                    SERVICE_WORKER_PROVIDER_FOR_CONTROLLEE,
                                    helper_->context()->AsWeakPtr(),
                                    NULL));
  version_->AddControllee(host.get());
  EXPECT_TRUE(version_->stop_worker_timer_.IsRunning());
}

TEST_F(ServiceWorkerVersionTest, ListenerAvailability) {
  // Initially the worker is not running. There should be no cache_listener_.
  EXPECT_FALSE(version_->cache_listener_.get());

  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(
      CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();

  // A new cache listener should be available once the worker starts.
  EXPECT_TRUE(version_->cache_listener_.get());

  version_->StopWorker(
      CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();

  // Should be destroyed when the worker stops.
  EXPECT_FALSE(version_->cache_listener_.get());

  version_->StartWorker(
      CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();

  // Recreated when the worker starts again.
  EXPECT_TRUE(version_->cache_listener_.get());
}

TEST_F(ServiceWorkerFailToStartTest, RendererCrash) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value
  version_->StartWorker(
      CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();

  // Callback has not completed yet.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NETWORK, status);
  EXPECT_EQ(ServiceWorkerVersion::STARTING, version_->running_status());

  // Simulate renderer crash: do what
  // ServiceWorkerDispatcherHost::OnFilterRemoved does.
  int process_id = helper_->mock_render_process_id();
  helper_->context()->RemoveAllProviderHostsForProcess(process_id);
  helper_->context()->embedded_worker_registry()->RemoveChildProcessSender(
      process_id);
  base::RunLoop().RunUntilIdle();

  // Callback completed.
  EXPECT_EQ(SERVICE_WORKER_ERROR_START_WORKER_FAILED, status);
  EXPECT_EQ(ServiceWorkerVersion::STOPPED, version_->running_status());
}

}  // namespace content
