// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_dispatcher_host.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/common/service_worker_messages.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MessageLoop;

namespace content {

class ServiceWorkerDispatcherHostTest : public testing::Test {
 protected:
  virtual void SetUp() {
    context_.reset(new ServiceWorkerContextCore(base::FilePath(), NULL));
  }

  virtual void TearDown() {
    context_.reset();
  }

  scoped_ptr<ServiceWorkerContextCore> context_;
};

static const int kRenderProcessId = 1;

class TestingServiceWorkerDispatcherHost : public ServiceWorkerDispatcherHost {
 public:
  TestingServiceWorkerDispatcherHost(int process_id,
                                     ServiceWorkerContextCore* context)
      : ServiceWorkerDispatcherHost(process_id) {
    context_ = context->AsWeakPtr();
  }

  virtual bool Send(IPC::Message* message) OVERRIDE {
    sent_messages_.push_back(message);
    return true;
  }

  ScopedVector<IPC::Message> sent_messages_;

 protected:
  virtual ~TestingServiceWorkerDispatcherHost() {}
};

TEST_F(ServiceWorkerDispatcherHostTest, DisabledCausesError) {
  DCHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableServiceWorker));

  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host =
      new TestingServiceWorkerDispatcherHost(kRenderProcessId, context_.get());

  bool handled;
  dispatcher_host->OnMessageReceived(
      ServiceWorkerHostMsg_RegisterServiceWorker(-1, -1, GURL(), GURL()),
      &handled);
  DCHECK(handled);

  // TODO(alecflett): Pump the message loop when this becomes async.
  DCHECK_EQ(1UL, dispatcher_host->sent_messages_.size());
  DCHECK_EQ(
      static_cast<uint32>(ServiceWorkerMsg_ServiceWorkerRegistrationError::ID),
      dispatcher_host->sent_messages_[0]->type());
}

TEST_F(ServiceWorkerDispatcherHostTest, Enabled) {
  DCHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableServiceWorker));
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableServiceWorker);

  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host =
      new TestingServiceWorkerDispatcherHost(kRenderProcessId, context_.get());

  bool handled;
  dispatcher_host->OnMessageReceived(
      ServiceWorkerHostMsg_RegisterServiceWorker(-1, -1, GURL(), GURL()),
      &handled);
  DCHECK(handled);

  // TODO(alecflett): Pump the message loop when this becomes async.
  DCHECK_EQ(1UL, dispatcher_host->sent_messages_.size());
  DCHECK_EQ(static_cast<uint32>(ServiceWorkerMsg_ServiceWorkerRegistered::ID),
            dispatcher_host->sent_messages_[0]->type());
}

TEST_F(ServiceWorkerDispatcherHostTest, EarlyContextDeletion) {
  DCHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableServiceWorker));
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableServiceWorker);

  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host =
      new TestingServiceWorkerDispatcherHost(kRenderProcessId, context_.get());

  context_.reset();

  bool handled;
  dispatcher_host->OnMessageReceived(
      ServiceWorkerHostMsg_RegisterServiceWorker(-1, -1, GURL(), GURL()),
      &handled);
  DCHECK(handled);

  // TODO(alecflett): Pump the message loop when this becomes async.
  DCHECK_EQ(1UL, dispatcher_host->sent_messages_.size());
  DCHECK_EQ(
      static_cast<uint32>(ServiceWorkerMsg_ServiceWorkerRegistrationError::ID),
      dispatcher_host->sent_messages_[0]->type());
}

}  // namespace content
