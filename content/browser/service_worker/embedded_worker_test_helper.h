// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/mojo/service_registry_impl.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

struct EmbeddedWorkerMsg_StartWorker_Params;
class GURL;

namespace content {

class EmbeddedWorkerRegistry;
class EmbeddedWorkerTestHelper;
class MessagePortMessageFilter;
class MockRenderProcessHost;
struct PushEventPayload;
class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;
struct ServiceWorkerFetchRequest;
class TestBrowserContext;

// In-Process EmbeddedWorker test helper.
//
// Usage: create an instance of this class to test browser-side embedded worker
// code without creating a child process.  This class will create a
// ServiceWorkerContextWrapper and ServiceWorkerContextCore for you.
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
  // If |user_data_directory| is empty, the context makes storage stuff in
  // memory.
  explicit EmbeddedWorkerTestHelper(const base::FilePath& user_data_directory);
  ~EmbeddedWorkerTestHelper() override;

  // Call this to simulate add/associate a process to a pattern.
  // This also registers this sender for the process.
  void SimulateAddProcessToPattern(const GURL& pattern, int process_id);

  // IPC::Sender implementation.
  bool Send(IPC::Message* message) override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  // IPC sink for EmbeddedWorker messages.
  IPC::TestSink* ipc_sink() { return &sink_; }
  // Inner IPC sink for script context messages sent via EmbeddedWorker.
  IPC::TestSink* inner_ipc_sink() { return &inner_sink_; }

  ServiceWorkerContextCore* context();
  ServiceWorkerContextWrapper* context_wrapper() { return wrapper_.get(); }
  void ShutdownContext();

  int GetNextThreadId() { return next_thread_id_++; }

  int mock_render_process_id() const { return mock_render_process_id_;}
  MockRenderProcessHost* mock_render_process_host() {
    return render_process_host_.get();
  }

  std::map<int, int64_t> embedded_worker_id_service_worker_version_id_map() {
    return embedded_worker_id_service_worker_version_id_map_;
  }

  // Only used for tests that force creating a new render process. There is no
  // corresponding MockRenderProcessHost.
  int new_render_process_id() const { return mock_render_process_id_ + 1; }

  TestBrowserContext* browser_context() { return browser_context_.get(); }

 protected:
  // Called when StartWorker, StopWorker and SendMessageToWorker message
  // is sent to the embedded worker. Override if necessary. By default
  // they verify given parameters and:
  // - OnStartWorker calls SimulateWorkerStarted
  // - OnStopWorker calls SimulateWorkerStoped
  // - OnSendMessageToWorker calls the message's respective On*Event handler
  virtual void OnStartWorker(int embedded_worker_id,
                             int64_t service_worker_version_id,
                             const GURL& scope,
                             const GURL& script_url);
  virtual void OnStopWorker(int embedded_worker_id);
  virtual bool OnMessageToWorker(int thread_id,
                                 int embedded_worker_id,
                                 const IPC::Message& message);

  // Called to setup mojo for a new embedded worker. Override to register
  // services the worker should expose to the browser.
  virtual void OnSetupMojo(ServiceRegistry* service_registry);

  // On*Event handlers. Called by the default implementation of
  // OnMessageToWorker when events are sent to the embedded
  // worker. By default they just return success via
  // SimulateSendReplyToBrowser.
  virtual void OnActivateEvent(int embedded_worker_id, int request_id);
  virtual void OnInstallEvent(int embedded_worker_id, int request_id);
  virtual void OnFetchEvent(int embedded_worker_id,
                            int request_id,
                            const ServiceWorkerFetchRequest& request);
  virtual void OnPushEvent(int embedded_worker_id,
                           int request_id,
                           const PushEventPayload& payload);

  // These functions simulate sending an EmbeddedHostMsg message to the
  // browser.
  void SimulateWorkerReadyForInspection(int embedded_worker_id);
  void SimulateWorkerScriptCached(int embedded_worker_id);
  void SimulateWorkerScriptLoaded(int embedded_worker_id);
  void SimulateWorkerThreadStarted(int thread_id, int embedded_worker_id);
  void SimulateWorkerScriptEvaluated(int embedded_worker_id, bool success);
  void SimulateWorkerStarted(int embedded_worker_id);
  void SimulateWorkerStopped(int embedded_worker_id);
  void SimulateSend(IPC::Message* message);

  EmbeddedWorkerRegistry* registry();

 private:
  class MockEmbeddedWorkerSetup;

  void OnStartWorkerStub(const EmbeddedWorkerMsg_StartWorker_Params& params);
  void OnStopWorkerStub(int embedded_worker_id);
  void OnMessageToWorkerStub(int thread_id,
                             int embedded_worker_id,
                             const IPC::Message& message);
  void OnActivateEventStub(int request_id);
  void OnInstallEventStub(int request_id);
  void OnFetchEventStub(int request_id,
                        const ServiceWorkerFetchRequest& request);
  void OnPushEventStub(int request_id, const PushEventPayload& payload);
  void OnSetupMojoStub(
      int thread_id,
      mojo::shell::mojom::InterfaceProviderRequest services,
      mojo::shell::mojom::InterfaceProviderPtr exposed_services);

  MessagePortMessageFilter* NewMessagePortMessageFilter();

  scoped_ptr<TestBrowserContext> browser_context_;
  scoped_ptr<MockRenderProcessHost> render_process_host_;

  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;

  IPC::TestSink sink_;
  IPC::TestSink inner_sink_;

  int next_thread_id_;
  int mock_render_process_id_;

  ServiceRegistryImpl render_process_service_registry_;

  std::map<int, int64_t> embedded_worker_id_service_worker_version_id_map_;

  // Stores the ServiceRegistries that are associated with each individual
  // service worker.
  base::ScopedPtrHashMap<int, scoped_ptr<ServiceRegistryImpl>>
      thread_id_service_registry_map_;

  // Updated each time MessageToWorker message is received.
  int current_embedded_worker_id_;

  std::vector<scoped_refptr<MessagePortMessageFilter>>
      message_port_message_filters_;

  base::WeakPtrFactory<EmbeddedWorkerTestHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerTestHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_
