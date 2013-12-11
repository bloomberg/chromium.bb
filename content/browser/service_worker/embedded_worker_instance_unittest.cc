// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/stl_util.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/common/service_worker_messages.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

typedef std::vector<IPC::Message*> MessageList;

class FakeSender : public IPC::Sender {
 public:
  FakeSender() {}
  virtual ~FakeSender() {
    STLDeleteContainerPointers(sent_messages_.begin(), sent_messages_.end());
  }

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* message) OVERRIDE {
    sent_messages_.push_back(message);
    return true;
  }

  const MessageList& sent_messages() { return sent_messages_; }

 private:
  MessageList sent_messages_;
  DISALLOW_COPY_AND_ASSIGN(FakeSender);
};

}  // namespace

class EmbeddedWorkerInstanceTest : public testing::Test {
 protected:
  EmbeddedWorkerInstanceTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual void SetUp() OVERRIDE {
    context_.reset(new ServiceWorkerContextCore(base::FilePath(), NULL));
  }

  virtual void TearDown() OVERRIDE {
    context_.reset();
  }

  EmbeddedWorkerRegistry* embedded_worker_registry() {
    DCHECK(context_);
    return context_->embedded_worker_registry();
  }

  TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<ServiceWorkerContextCore> context_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstanceTest);
};

TEST_F(EmbeddedWorkerInstanceTest, StartAndStop) {
  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  FakeSender fake_sender;
  const int process_id = 11;
  const int thread_id = 33;
  const int64 service_worker_version_id = 55L;
  const GURL url("http://example.com/worker.js");

  // This fails as we have no available process yet.
  EXPECT_FALSE(worker->Start(service_worker_version_id, url));
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  // Simulate adding one process to the worker.
  worker->AddProcessReference(process_id);
  embedded_worker_registry()->AddChildProcessSender(process_id, &fake_sender);

  // Start should succeed.
  EXPECT_TRUE(worker->Start(service_worker_version_id, url));
  EXPECT_EQ(EmbeddedWorkerInstance::STARTING, worker->status());

  // Simulate an upcall from embedded worker to notify that it's started.
  worker->OnStarted(thread_id);
  EXPECT_EQ(EmbeddedWorkerInstance::RUNNING, worker->status());
  EXPECT_EQ(process_id, worker->process_id());
  EXPECT_EQ(thread_id, worker->thread_id());

  // Stop the worker.
  EXPECT_TRUE(worker->Stop());
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPING, worker->status());

  // Simulate an upcall from embedded worker to notify that it's stopped.
  worker->OnStopped();
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  // Verify that we've sent two messages to start and terminate the worker.
  const MessageList& messages = fake_sender.sent_messages();
  ASSERT_EQ(2U, messages.size());
  ASSERT_EQ(ServiceWorkerMsg_StartWorker::ID, messages[0]->type());
  ASSERT_EQ(ServiceWorkerMsg_TerminateWorker::ID, messages[1]->type());
}

TEST_F(EmbeddedWorkerInstanceTest, ChooseProcess) {
  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  FakeSender fake_sender;

  // Simulate adding processes to the worker.
  // Process 1 has 1 ref, 2 has 2 refs and 3 has 3 refs.
  worker->AddProcessReference(1);
  worker->AddProcessReference(2);
  worker->AddProcessReference(2);
  worker->AddProcessReference(3);
  worker->AddProcessReference(3);
  worker->AddProcessReference(3);
  embedded_worker_registry()->AddChildProcessSender(1, &fake_sender);
  embedded_worker_registry()->AddChildProcessSender(2, &fake_sender);
  embedded_worker_registry()->AddChildProcessSender(3, &fake_sender);

  // Process 3 has the biggest # of references and it should be chosen.
  EXPECT_TRUE(worker->Start(1L, GURL("http://example.com/worker.js")));
  EXPECT_EQ(EmbeddedWorkerInstance::STARTING, worker->status());
  EXPECT_EQ(3, worker->process_id());
}

}  // namespace content
