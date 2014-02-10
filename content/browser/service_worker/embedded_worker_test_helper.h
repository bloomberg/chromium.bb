// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class GURL;

namespace content {

class EmbeddedWorkerRegistry;
class EmbeddedWorkerTestHelper;
class ServiceWorkerContextCore;

// In-Process EmbeddedWorker test helper.
//
// Create an instance of this class for a ServiceWorkerContextCore,
// set up process association by calling Simulate{Add,Remove}Process
// and test to interact with an embedded worker without creating
// a child process.
//
// By default this class just notifies back WorkerStarted and WorkerStopped
// for StartWorker and StopWorker requests, and ignores any messages sent
// to the worker.
//
// Alternatively consumers can subclass this helper and override
// OnStartWorker(), OnStopWorker() and OnSendMessageToWorker() to add
// their own logic/verification code.
//
// See embedded_worker_instance_unittest.cc for example usages.
//
class EmbeddedWorkerTestHelper : public IPC::Sender,
                                 public IPC::Listener {
 public:
  // Initialize this helper for |context|, and enable this as an IPC
  // sender for |mock_render_process_id|.
  EmbeddedWorkerTestHelper(ServiceWorkerContextCore* context,
                           int mock_render_process_id);
  virtual ~EmbeddedWorkerTestHelper();

  // Call this to simulate add/associate a process to a worker.
  // This also registers this sender for the process.
  void SimulateAddProcessToWorker(int embedded_worker_id, int process_id);

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* message) OVERRIDE;

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  IPC::TestSink* ipc_sink() { return &sink_; }

 protected:
  // Called when StartWorker, StopWorker and SendMessageToWorker message
  // is sent to the embedded worker. Override if necessary. By default
  // they verify given parameters and:
  // - call SimulateWorkerStarted for OnStartWorker
  // - call SimulateWorkerStoped for OnStopWorker
  // - do nothing for OnSendMessageToWorker
  virtual void OnStartWorker(int embedded_worker_id,
                             int64 service_worker_version_id,
                             const GURL& script_url);
  virtual void OnStopWorker(int embedded_worker_id);
  virtual void OnSendMessageToWorker(int thread_id,
                                     int embedded_worker_id,
                                     int request_id,
                                     const IPC::Message& message);

  // Call this to simulate sending WorkerStarted, WorkerStopped and
  // SendMessageToBrowser to the browser.
  void SimulateWorkerStarted(int thread_id, int embedded_worker_id);
  void SimulateWorkerStopped(int embedded_worker_id);
  void SimulateSendMessageToBrowser(int embedded_worker_id,
                                    int request_id,
                                    const IPC::Message& message);

 private:
  void OnStartWorkerStub(int embedded_worker_id,
                         int64 service_worker_version_id,
                         const GURL& script_url);
  void OnStopWorkerStub(int embedded_worker_id);
  void OnSendMessageToWorkerStub(int thread_id,
                                 int embedded_worker_id,
                                 int request_id,
                                 const IPC::Message& message);

  EmbeddedWorkerRegistry* registry();

  base::WeakPtr<ServiceWorkerContextCore> context_;
  IPC::TestSink sink_;
  int next_thread_id_;
  std::vector<IPC::Message> messages_;
  base::WeakPtrFactory<EmbeddedWorkerTestHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerTestHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_
