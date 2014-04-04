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
struct ServiceWorkerFetchRequest;

// In-Process EmbeddedWorker test helper.
//
// Usage: create an instance of this class for a ServiceWorkerContextCore
// to test browser-side embedded worker code without creating a child process.
//
// By default this class just notifies back WorkerStarted and WorkerStopped
// for StartWorker and StopWorker requests. The default implementation
// also returns success for event messages (e.g. InstallEvent, FetchEvent).
//
// Alternatively consumers can subclass this helper and override On*()
// methods to add their own logic/verification code.
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

  // IPC sink for EmbeddedWorker messages.
  IPC::TestSink* ipc_sink() { return &sink_; }
  // Inner IPC sink for script context messages sent via EmbeddedWorker.
  IPC::TestSink* inner_ipc_sink() { return &inner_sink_; }

 protected:
  // Called when StartWorker, StopWorker and SendMessageToWorker message
  // is sent to the embedded worker. Override if necessary. By default
  // they verify given parameters and:
  // - OnStartWorker calls SimulateWorkerStarted
  // - OnStopWorker calls SimulateWorkerStoped
  // - OnSendMessageToWorker calls the message's respective On*Event handler
  virtual void OnStartWorker(int embedded_worker_id,
                             int64 service_worker_version_id,
                             const GURL& script_url);
  virtual void OnStopWorker(int embedded_worker_id);
  virtual bool OnSendMessageToWorker(int thread_id,
                                     int embedded_worker_id,
                                     int request_id,
                                     const IPC::Message& message);

  // On*Event handlers. Called by the default implementation of
  // OnSendMessageToWorker when events are sent to the embedded
  // worker. By default they just return success via
  // SimulateSendMessageToBrowser.
  virtual void OnActivateEvent(int embedded_worker_id, int request_id);
  virtual void OnInstallEvent(int embedded_worker_id,
                              int request_id,
                              int active_version_id);
  virtual void OnFetchEvent(int embedded_worker_id,
                            int request_id,
                            const ServiceWorkerFetchRequest& request);

  // Call this to simulate sending WorkerStarted, WorkerStopped and
  // SendMessageToBrowser to the browser.
  void SimulateWorkerStarted(int thread_id, int embedded_worker_id);
  void SimulateWorkerStopped(int embedded_worker_id);
  void SimulateSendMessageToBrowser(int embedded_worker_id,
                                    int request_id,
                                    const IPC::Message& message);

 protected:
  EmbeddedWorkerRegistry* registry();

 private:
  void OnStartWorkerStub(int embedded_worker_id,
                         int64 service_worker_version_id,
                         const GURL& script_url);
  void OnStopWorkerStub(int embedded_worker_id);
  void OnSendMessageToWorkerStub(int thread_id,
                                 int embedded_worker_id,
                                 int request_id,
                                 const IPC::Message& message);
  void OnActivateEventStub();
  void OnInstallEventStub(int active_version_id);
  void OnFetchEventStub(const ServiceWorkerFetchRequest& request);

  base::WeakPtr<ServiceWorkerContextCore> context_;

  IPC::TestSink sink_;
  IPC::TestSink inner_sink_;

  int next_thread_id_;

  // Updated each time SendMessageToWorker message is received.
  int current_embedded_worker_id_;
  int current_request_id_;

  base::WeakPtrFactory<EmbeddedWorkerTestHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerTestHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_
