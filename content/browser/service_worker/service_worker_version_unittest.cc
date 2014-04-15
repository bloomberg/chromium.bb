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
IPC_MESSAGE_CONTROL1(TestMsg_Request, int);
IPC_MESSAGE_CONTROL1(TestMsg_Response, int);

// ---------------------------------------------------------------------------

namespace content {

namespace {

static const int kRenderProcessId = 1;

class MessageReceiver : public EmbeddedWorkerTestHelper {
 public:
  MessageReceiver(ServiceWorkerContextCore* context)
      : EmbeddedWorkerTestHelper(context, kRenderProcessId),
        current_embedded_worker_id_(0),
        current_request_id_(0) {}
  virtual ~MessageReceiver() {}

  virtual bool OnSendMessageToWorker(int thread_id,
                                     int embedded_worker_id,
                                     int request_id,
                                     const IPC::Message& message) OVERRIDE {
    if (EmbeddedWorkerTestHelper::OnSendMessageToWorker(
            thread_id, embedded_worker_id, request_id, message)) {
      return true;
    }
    current_embedded_worker_id_ = embedded_worker_id;
    current_request_id_ = request_id;
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MessageReceiver, message)
      IPC_MESSAGE_HANDLER(TestMsg_Message, OnMessage)
      IPC_MESSAGE_HANDLER(TestMsg_Request, OnRequest)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

 private:
  void OnMessage() {
    // Do nothing.
  }

  void OnRequest(int value) {
    // Double the given value and send back the response.
    SimulateSendMessageToBrowser(current_embedded_worker_id_,
                                 current_request_id_,
                                 TestMsg_Response(value * 2));
  }

  int current_embedded_worker_id_;
  int current_request_id_;
  DISALLOW_COPY_AND_ASSIGN(MessageReceiver);
};

void ReceiveResponse(ServiceWorkerStatusCode* status_out,
                     int* value_out,
                     ServiceWorkerStatusCode status,
                     const IPC::Message& message) {
  Tuple1<int> param;
  ASSERT_TRUE(TestMsg_Response::Read(&message, &param));
  *status_out = status;
  *value_out = param.a;
}

void VerifyCalled(bool* called) {
  *called = true;
}

void ObserveStatusChanges(ServiceWorkerVersion* version,
                          std::vector<ServiceWorkerVersion::Status>* statuses) {
  statuses->push_back(version->status());
  version->RegisterStatusChangeCallback(
      base::Bind(&ObserveStatusChanges, base::Unretained(version), statuses));
}

}  // namespace

class ServiceWorkerVersionTest : public testing::Test {
 protected:
  ServiceWorkerVersionTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual void SetUp() OVERRIDE {
    context_.reset(new ServiceWorkerContextCore(base::FilePath(), NULL));
    helper_.reset(new MessageReceiver(context_.get()));

    registration_ = new ServiceWorkerRegistration(
        GURL("http://www.example.com/*"),
        GURL("http://www.example.com/service_worker.js"),
        1L, context_->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_,
        1L, context_->AsWeakPtr());

    // Simulate adding one process to the worker.
    int embedded_worker_id = version_->embedded_worker()->embedded_worker_id();
    helper_->SimulateAddProcessToWorker(embedded_worker_id, kRenderProcessId);
    ASSERT_TRUE(version_->HasProcessToRun());
  }

  virtual void TearDown() OVERRIDE {
    version_ = 0;
    registration_ = 0;
    helper_.reset();
    context_.reset();
  }

  TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<ServiceWorkerContextCore> context_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerVersionTest);
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
  status3 = SERVICE_WORKER_ERROR_FAILED;
  version_->StopWorker(CreateReceiverOnCurrentThread(&status1));
  version_->StopWorker(CreateReceiverOnCurrentThread(&status2));

  // Also try calling StartWorker while StopWorker is in queue.
  version_->StartWorker(CreateReceiverOnCurrentThread(&status3));

  EXPECT_EQ(ServiceWorkerVersion::STOPPING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ServiceWorkerVersion::STOPPED, version_->running_status());

  // All StopWorker should just succeed, while StartWorker fails.
  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);
  EXPECT_EQ(SERVICE_WORKER_ERROR_START_WORKER_FAILED, status3);
}

TEST_F(ServiceWorkerVersionTest, SendMessage) {
  EXPECT_EQ(ServiceWorkerVersion::STOPPED, version_->running_status());

  // Send a message without starting the worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->SendMessage(TestMsg_Message(),
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  // The worker should be now started.
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());

  // Stop the worker, and then send the message immediately.
  ServiceWorkerStatusCode msg_status = SERVICE_WORKER_ERROR_FAILED;
  ServiceWorkerStatusCode stop_status = SERVICE_WORKER_ERROR_FAILED;
  version_->StopWorker(CreateReceiverOnCurrentThread(&stop_status));
  version_->SendMessage(TestMsg_Message(),
                       CreateReceiverOnCurrentThread(&msg_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, stop_status);

  // SendMessage should return START_WORKER_FAILED error since it tried to
  // start a worker while it was stopping.
  EXPECT_EQ(SERVICE_WORKER_ERROR_START_WORKER_FAILED, msg_status);
}

TEST_F(ServiceWorkerVersionTest, ReSendMessageAfterStop) {
  EXPECT_EQ(ServiceWorkerVersion::STOPPED, version_->running_status());

  // Start the worker.
  ServiceWorkerStatusCode start_status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(CreateReceiverOnCurrentThread(&start_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, start_status);
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());

  // Stop the worker, and then send the message immediately.
  ServiceWorkerStatusCode msg_status = SERVICE_WORKER_ERROR_FAILED;
  ServiceWorkerStatusCode stop_status = SERVICE_WORKER_ERROR_FAILED;
  version_->StopWorker(CreateReceiverOnCurrentThread(&stop_status));
  version_->SendMessage(TestMsg_Message(),
                       CreateReceiverOnCurrentThread(&msg_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, stop_status);

  // SendMessage should return START_WORKER_FAILED error since it tried to
  // start a worker while it was stopping.
  EXPECT_EQ(SERVICE_WORKER_ERROR_START_WORKER_FAILED, msg_status);

  // Resend the message, which should succeed and restart the worker.
  version_->SendMessage(TestMsg_Message(),
                       CreateReceiverOnCurrentThread(&msg_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, msg_status);
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, SendMessageAndRegisterCallback) {
  // Send multiple messages and verify responses.
  ServiceWorkerStatusCode status1 = SERVICE_WORKER_ERROR_FAILED;
  ServiceWorkerStatusCode status2 = SERVICE_WORKER_ERROR_FAILED;
  int value1 = -1, value2 = -1;

  version_->SendMessageAndRegisterCallback(
      TestMsg_Request(111),
      base::Bind(&ReceiveResponse, &status1, &value1));
  version_->SendMessageAndRegisterCallback(
      TestMsg_Request(333),
      base::Bind(&ReceiveResponse, &status2, &value2));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);
  EXPECT_EQ(111 * 2, value1);
  EXPECT_EQ(333 * 2, value2);
}

TEST_F(ServiceWorkerVersionTest, InstallAndWaitCompletion) {
  EXPECT_EQ(ServiceWorkerVersion::NEW, version_->status());

  // Dispatch an install event.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->DispatchInstallEvent(-1, CreateReceiverOnCurrentThread(&status));
  EXPECT_EQ(ServiceWorkerVersion::INSTALLING, version_->status());

  // Wait for the completion.
  bool status_change_called = false;
  version_->RegisterStatusChangeCallback(
      base::Bind(&VerifyCalled, &status_change_called));

  base::RunLoop().RunUntilIdle();

  // After successful completion, version's status must be changed to
  // INSTALLED, and status change callback must have been fired.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(status_change_called);
  EXPECT_EQ(ServiceWorkerVersion::INSTALLED, version_->status());
}

TEST_F(ServiceWorkerVersionTest, ActivateAndWaitCompletion) {
  version_->SetStatus(ServiceWorkerVersion::INSTALLED);
  EXPECT_EQ(ServiceWorkerVersion::INSTALLED, version_->status());

  // Dispatch an activate event.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->DispatchActivateEvent(CreateReceiverOnCurrentThread(&status));
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATING, version_->status());

  // Wait for the completion.
  bool status_change_called = false;
  version_->RegisterStatusChangeCallback(
      base::Bind(&VerifyCalled, &status_change_called));

  base::RunLoop().RunUntilIdle();

  // After successful completion, version's status must be changed to
  // ACTIVE, and status change callback must have been fired.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(status_change_called);
  EXPECT_EQ(ServiceWorkerVersion::ACTIVE, version_->status());
}

TEST_F(ServiceWorkerVersionTest, RepeatedlyObserveStatusChanges) {
  EXPECT_EQ(ServiceWorkerVersion::NEW, version_->status());

  // Repeatedly observe status changes (the callback re-registers itself).
  std::vector<ServiceWorkerVersion::Status> statuses;
  version_->RegisterStatusChangeCallback(
      base::Bind(&ObserveStatusChanges, version_, &statuses));

  // Dispatch some events.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->DispatchInstallEvent(-1, CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  status = SERVICE_WORKER_ERROR_FAILED;
  version_->DispatchActivateEvent(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  // Verify that we could successfully observe repeated status changes.
  ASSERT_EQ(4U, statuses.size());
  ASSERT_EQ(ServiceWorkerVersion::INSTALLING, statuses[0]);
  ASSERT_EQ(ServiceWorkerVersion::INSTALLED, statuses[1]);
  ASSERT_EQ(ServiceWorkerVersion::ACTIVATING, statuses[2]);
  ASSERT_EQ(ServiceWorkerVersion::ACTIVE, statuses[3]);
}

TEST_F(ServiceWorkerVersionTest, AddAndRemoveProcesses) {
  // Preparation (to reset the process count to 0).
  ASSERT_TRUE(version_->HasProcessToRun());
  version_->RemoveProcessFromWorker(kRenderProcessId);
  ASSERT_FALSE(version_->HasProcessToRun());

  // Add another process to the worker twice, and then remove process once.
  const int another_process_id = kRenderProcessId + 1;
  version_->AddProcessToWorker(another_process_id);
  version_->AddProcessToWorker(another_process_id);
  version_->RemoveProcessFromWorker(another_process_id);

  // We're ref-counting the process internally, so adding the same process
  // multiple times should be handled correctly.
  ASSERT_TRUE(version_->HasProcessToRun());

  // Removing the process again (so that # of AddProcess == # of RemoveProcess
  // for the process) should remove all process references.
  version_->RemoveProcessFromWorker(another_process_id);
  ASSERT_FALSE(version_->HasProcessToRun());
}

}  // namespace content
