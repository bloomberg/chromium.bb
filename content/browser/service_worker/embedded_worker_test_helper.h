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
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/common/service_worker/embedded_worker.mojom.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_test_sink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/http/http_response_info.h"
#include "url/gurl.h"

class GURL;

namespace service_manager {
class InterfaceRegistry;
}

namespace content {

class EmbeddedWorkerRegistry;
class EmbeddedWorkerTestHelper;
class MockRenderProcessHost;
class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;
class ServiceWorkerDispatcherHost;
class TestBrowserContext;
struct EmbeddedWorkerStartParams;
struct PlatformNotificationData;
struct PushEventPayload;
struct ServiceWorkerFetchRequest;

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
  enum class Event { Activate };
  using FetchCallback =
      base::Callback<void(ServiceWorkerStatusCode,
                          base::Time /* dispatch_event_time */)>;

  class MockEmbeddedWorkerInstanceClient
      : public mojom::EmbeddedWorkerInstanceClient {
   public:
    explicit MockEmbeddedWorkerInstanceClient(
        base::WeakPtr<EmbeddedWorkerTestHelper> helper);
    ~MockEmbeddedWorkerInstanceClient() override;

    static void Bind(const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
                     mojom::EmbeddedWorkerInstanceClientRequest request);

   protected:
    // Implementation of mojo interfaces.
    void StartWorker(
        const EmbeddedWorkerStartParams& params,
        mojom::ServiceWorkerEventDispatcherRequest dispatcher_request) override;
    void StopWorker(const StopWorkerCallback& callback) override;
    void ResumeAfterDownload() override;
    void AddMessageToConsole(blink::WebConsoleMessage::Level level,
                             const std::string& message) override;

    base::WeakPtr<EmbeddedWorkerTestHelper> helper_;
    mojo::Binding<mojom::EmbeddedWorkerInstanceClient> binding_;

    base::Optional<int> embedded_worker_id_;

   private:
    DISALLOW_COPY_AND_ASSIGN(MockEmbeddedWorkerInstanceClient);
  };

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

  // Register a mojo endpoint object derived from
  // MockEmbeddedWorkerInstanceClient.
  void RegisterMockInstanceClient(
      std::unique_ptr<MockEmbeddedWorkerInstanceClient> client);

  template <typename MockType, typename... Args>
  MockType* CreateAndRegisterMockInstanceClient(Args&&... args);

  // IPC sink for EmbeddedWorker messages.
  IPC::TestSink* ipc_sink() { return &sink_; }
  // Inner IPC sink for script context messages sent via EmbeddedWorker.
  IPC::TestSink* inner_ipc_sink() { return &inner_sink_; }

  std::vector<Event>* dispatched_events() { return &events_; }

  std::vector<std::unique_ptr<MockEmbeddedWorkerInstanceClient>>*
  mock_instance_clients() {
    return &mock_instance_clients_;
  }

  ServiceWorkerContextCore* context();
  ServiceWorkerContextWrapper* context_wrapper() { return wrapper_.get(); }
  void ShutdownContext();

  int GetNextThreadId() { return next_thread_id_++; }

  int mock_render_process_id() const { return mock_render_process_id_; }
  MockRenderProcessHost* mock_render_process_host() {
    return render_process_host_.get();
  }

  std::map<int, int64_t> embedded_worker_id_service_worker_version_id_map() {
    return embedded_worker_id_service_worker_version_id_map_;
  }

  // Only used for tests that force creating a new render process.
  int new_render_process_id() const { return new_mock_render_process_id_; }

  TestBrowserContext* browser_context() { return browser_context_.get(); }

  base::WeakPtr<EmbeddedWorkerTestHelper> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  static net::HttpResponseInfo CreateHttpResponseInfo();

 protected:
  // StartWorker IPC handler routed through MockEmbeddedWorkerInstanceClient.
  // This simulates each legacy IPC sent from the renderer and binds |request|
  // to MockServiceWorkerEventDispatcher by default.
  virtual void OnStartWorker(
      int embedded_worker_id,
      int64_t service_worker_version_id,
      const GURL& scope,
      const GURL& script_url,
      bool pause_after_download,
      mojom::ServiceWorkerEventDispatcherRequest request);
  virtual void OnResumeAfterDownload(int embedded_worker_id);
  // StopWorker IPC handler routed through MockEmbeddedWorkerInstanceClient.
  // This calls StopWorkerCallback by default.
  virtual void OnStopWorker(
      const mojom::EmbeddedWorkerInstanceClient::StopWorkerCallback& callback);
  // The legacy IPC message handler. This passes the messages to their
  // respective On*Event handler by default.
  virtual bool OnMessageToWorker(int thread_id,
                                 int embedded_worker_id,
                                 const IPC::Message& message);

  // On*Event handlers. Called by the default implementation of
  // OnMessageToWorker when events are sent to the embedded
  // worker. By default they just return success via
  // SimulateSendReplyToBrowser.
  virtual void OnActivateEvent(
      const mojom::ServiceWorkerEventDispatcher::DispatchActivateEventCallback&
          callback);
  virtual void OnExtendableMessageEvent(
      mojom::ExtendableMessageEventPtr event,
      const mojom::ServiceWorkerEventDispatcher::
          DispatchExtendableMessageEventCallback& callback);
  virtual void OnInstallEvent(int embedded_worker_id, int request_id);
  virtual void OnFetchEvent(int embedded_worker_id,
                            int fetch_event_id,
                            const ServiceWorkerFetchRequest& request,
                            mojom::FetchEventPreloadHandlePtr preload_handle,
                            const FetchCallback& callback);
  virtual void OnNotificationClickEvent(
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      int action_index,
      const base::Optional<base::string16>& reply,
      const mojom::ServiceWorkerEventDispatcher::
          DispatchNotificationClickEventCallback& callback);
  virtual void OnNotificationCloseEvent(
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      const mojom::ServiceWorkerEventDispatcher::
          DispatchNotificationCloseEventCallback& callback);
  virtual void OnPushEvent(
      const PushEventPayload& payload,
      const mojom::ServiceWorkerEventDispatcher::DispatchPushEventCallback&
          callback);
  virtual void OnPaymentRequestEvent(
      payments::mojom::PaymentAppRequestPtr data,
      const mojom::ServiceWorkerEventDispatcher::
          DispatchPaymentRequestEventCallback& callback);

  // These functions simulate sending an EmbeddedHostMsg message through the
  // legacy IPC system to the browser.
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
  class MockServiceWorkerEventDispatcher;

  void OnStartWorkerStub(const EmbeddedWorkerStartParams& params,
                         mojom::ServiceWorkerEventDispatcherRequest request);
  void OnResumeAfterDownloadStub(int embedded_worker_id);
  void OnStopWorkerStub(
      const mojom::EmbeddedWorkerInstanceClient::StopWorkerCallback& callback);
  void OnMessageToWorkerStub(int thread_id,
                             int embedded_worker_id,
                             const IPC::Message& message);
  void OnActivateEventStub(
      const mojom::ServiceWorkerEventDispatcher::DispatchActivateEventCallback&
          callback);
  void OnExtendableMessageEventStub(
      mojom::ExtendableMessageEventPtr event,
      const mojom::ServiceWorkerEventDispatcher::
          DispatchExtendableMessageEventCallback& callback);
  void OnInstallEventStub(int request_id);
  void OnFetchEventStub(int thread_id,
                        int fetch_event_id,
                        const ServiceWorkerFetchRequest& request,
                        mojom::FetchEventPreloadHandlePtr preload_handle,
                        const FetchCallback& callback);
  void OnNotificationClickEventStub(
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      int action_index,
      const base::Optional<base::string16>& reply,
      const mojom::ServiceWorkerEventDispatcher::
          DispatchNotificationClickEventCallback& callback);
  void OnNotificationCloseEventStub(
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      const mojom::ServiceWorkerEventDispatcher::
          DispatchNotificationCloseEventCallback& callback);
  void OnPushEventStub(
      const PushEventPayload& payload,
      const mojom::ServiceWorkerEventDispatcher::DispatchPushEventCallback&
          callback);
  void OnPaymentRequestEventStub(
      payments::mojom::PaymentAppRequestPtr data,
      const mojom::ServiceWorkerEventDispatcher::
          DispatchPaymentRequestEventCallback& callback);

  std::unique_ptr<service_manager::InterfaceRegistry> CreateInterfaceRegistry(
      MockRenderProcessHost* rph);

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<MockRenderProcessHost> render_process_host_;
  std::unique_ptr<MockRenderProcessHost> new_render_process_host_;

  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;

  IPC::TestSink sink_;
  IPC::TestSink inner_sink_;

  std::vector<std::unique_ptr<MockEmbeddedWorkerInstanceClient>>
      mock_instance_clients_;
  size_t mock_instance_clients_next_index_;

  int next_thread_id_;
  int mock_render_process_id_;
  int new_mock_render_process_id_;

  std::map<int /* process_id */, scoped_refptr<ServiceWorkerDispatcherHost>>
      dispatcher_hosts_;

  std::unique_ptr<service_manager::InterfaceRegistry>
      render_process_interface_registry_;
  std::unique_ptr<service_manager::InterfaceRegistry>
      new_render_process_interface_registry_;

  std::map<int, int64_t> embedded_worker_id_service_worker_version_id_map_;
  std::map<int /* thread_id */, int /* embedded_worker_id */>
      thread_id_embedded_worker_id_map_;

  // Updated each time MessageToWorker message is received.
  int current_embedded_worker_id_;

  std::vector<Event> events_;

  base::WeakPtrFactory<EmbeddedWorkerTestHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerTestHelper);
};

template <typename MockType, typename... Args>
MockType* EmbeddedWorkerTestHelper::CreateAndRegisterMockInstanceClient(
    Args&&... args) {
  std::unique_ptr<MockType> mock =
      base::MakeUnique<MockType>(std::forward<Args>(args)...);
  MockType* mock_rawptr = mock.get();
  RegisterMockInstanceClient(std::move(mock));
  return mock_rawptr;
}

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_
