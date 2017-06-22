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
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/service_worker/embedded_worker.mojom.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_test_sink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/http/http_response_info.h"
#include "url/gurl.h"

class GURL;

namespace content {

struct BackgroundFetchSettledFetch;
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
  enum class Event { Install, Activate };
  using FetchCallback =
      base::OnceCallback<void(ServiceWorkerStatusCode,
                              base::Time /* dispatch_event_time */)>;

  class MockEmbeddedWorkerInstanceClient
      : public mojom::EmbeddedWorkerInstanceClient {
   public:
    explicit MockEmbeddedWorkerInstanceClient(
        base::WeakPtr<EmbeddedWorkerTestHelper> helper);
    ~MockEmbeddedWorkerInstanceClient() override;

    static void Bind(const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
                     mojo::ScopedMessagePipeHandle request);

   protected:
    // Implementation of mojo interfaces.
    void StartWorker(
        const EmbeddedWorkerStartParams& params,
        mojom::ServiceWorkerEventDispatcherRequest dispatcher_request,
        mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
        mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info)
        override;
    void StopWorker() override;
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

  // Registers a Mojo endpoint object derived from
  // MockEmbeddedWorkerInstanceClient.
  void RegisterMockInstanceClient(
      std::unique_ptr<MockEmbeddedWorkerInstanceClient> client);

  // Registers the dispatcher host for the process to a map managed by this test
  // helper. If there is a existing dispatcher host, it'll removed before adding
  // to the map. This should be called before ServiceWorkerDispatcherHost::Init
  // because it internally calls ServiceWorkerContextCore::AddDispatcherHost.
  // If |dispatcher_host| is nullptr, this method just removes the existing
  // dispatcher host from the map.
  void RegisterDispatcherHost(
      int process_id,
      scoped_refptr<ServiceWorkerDispatcherHost> dispatcher_host);

  template <typename MockType, typename... Args>
  MockType* CreateAndRegisterMockInstanceClient(Args&&... args);

  // IPC sink for EmbeddedWorker messages.
  IPC::TestSink* ipc_sink() { return &sink_; }

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
      mojom::ServiceWorkerEventDispatcherRequest request,
      mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
      mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info);
  virtual void OnResumeAfterDownload(int embedded_worker_id);
  // StopWorker IPC handler routed through MockEmbeddedWorkerInstanceClient.
  // This calls SimulateWorkerStopped() by default.
  virtual void OnStopWorker(int embedded_worker_id);

  // On*Event handlers. By default they just return success via
  // SimulateSendReplyToBrowser.
  virtual void OnActivateEvent(
      mojom::ServiceWorkerEventDispatcher::DispatchActivateEventCallback
          callback);
  virtual void OnBackgroundFetchAbortEvent(
      const std::string& tag,
      mojom::ServiceWorkerEventDispatcher::
          DispatchBackgroundFetchAbortEventCallback callback);
  virtual void OnBackgroundFetchClickEvent(
      const std::string& tag,
      mojom::BackgroundFetchState state,
      mojom::ServiceWorkerEventDispatcher::
          DispatchBackgroundFetchClickEventCallback callback);
  virtual void OnBackgroundFetchFailEvent(
      const std::string& tag,
      const std::vector<BackgroundFetchSettledFetch>& fetches,
      mojom::ServiceWorkerEventDispatcher::
          DispatchBackgroundFetchFailEventCallback callback);
  virtual void OnBackgroundFetchedEvent(
      const std::string& tag,
      const std::vector<BackgroundFetchSettledFetch>& fetches,
      mojom::ServiceWorkerEventDispatcher::
          DispatchBackgroundFetchedEventCallback callback);
  virtual void OnExtendableMessageEvent(
      mojom::ExtendableMessageEventPtr event,
      mojom::ServiceWorkerEventDispatcher::
          DispatchExtendableMessageEventCallback callback);
  virtual void OnInstallEvent(
      mojom::ServiceWorkerInstallEventMethodsAssociatedPtrInfo client,
      mojom::ServiceWorkerEventDispatcher::DispatchInstallEventCallback
          callback);
  virtual void OnFetchEvent(
      int embedded_worker_id,
      int fetch_event_id,
      const ServiceWorkerFetchRequest& request,
      mojom::FetchEventPreloadHandlePtr preload_handle,
      mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      FetchCallback finish_callback);
  virtual void OnNotificationClickEvent(
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      int action_index,
      const base::Optional<base::string16>& reply,
      mojom::ServiceWorkerEventDispatcher::
          DispatchNotificationClickEventCallback callback);
  virtual void OnNotificationCloseEvent(
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      mojom::ServiceWorkerEventDispatcher::
          DispatchNotificationCloseEventCallback callback);
  virtual void OnPushEvent(
      const PushEventPayload& payload,
      mojom::ServiceWorkerEventDispatcher::DispatchPushEventCallback callback);
  virtual void OnPaymentRequestEvent(
      payments::mojom::PaymentRequestEventDataPtr data,
      payments::mojom::PaymentAppResponseCallbackPtr response_callback,
      mojom::ServiceWorkerEventDispatcher::DispatchPaymentRequestEventCallback
          callback);

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

  void OnStartWorkerStub(
      const EmbeddedWorkerStartParams& params,
      mojom::ServiceWorkerEventDispatcherRequest request,
      mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
      mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info);
  void OnResumeAfterDownloadStub(int embedded_worker_id);
  void OnStopWorkerStub(int embedded_worker_id);
  void OnMessageToWorkerStub(int thread_id,
                             int embedded_worker_id,
                             const IPC::Message& message);
  void OnActivateEventStub(
      mojom::ServiceWorkerEventDispatcher::DispatchActivateEventCallback
          callback);
  void OnBackgroundFetchAbortEventStub(
      const std::string& tag,
      mojom::ServiceWorkerEventDispatcher::
          DispatchBackgroundFetchAbortEventCallback callback);
  void OnBackgroundFetchClickEventStub(
      const std::string& tag,
      mojom::BackgroundFetchState state,
      mojom::ServiceWorkerEventDispatcher::
          DispatchBackgroundFetchClickEventCallback callback);
  void OnBackgroundFetchFailEventStub(
      const std::string& tag,
      const std::vector<BackgroundFetchSettledFetch>& fetches,
      mojom::ServiceWorkerEventDispatcher::
          DispatchBackgroundFetchFailEventCallback callback);
  void OnBackgroundFetchedEventStub(
      const std::string& tag,
      const std::vector<BackgroundFetchSettledFetch>& fetches,
      mojom::ServiceWorkerEventDispatcher::
          DispatchBackgroundFetchedEventCallback callback);
  void OnExtendableMessageEventStub(
      mojom::ExtendableMessageEventPtr event,
      mojom::ServiceWorkerEventDispatcher::
          DispatchExtendableMessageEventCallback callback);
  void OnInstallEventStub(
      mojom::ServiceWorkerInstallEventMethodsAssociatedPtrInfo client,
      mojom::ServiceWorkerEventDispatcher::DispatchInstallEventCallback
          callback);
  void OnFetchEventStub(
      int thread_id,
      int fetch_event_id,
      const ServiceWorkerFetchRequest& request,
      mojom::FetchEventPreloadHandlePtr preload_handle,
      mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      FetchCallback finish_callback);
  void OnNotificationClickEventStub(
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      int action_index,
      const base::Optional<base::string16>& reply,
      mojom::ServiceWorkerEventDispatcher::
          DispatchNotificationClickEventCallback callback);
  void OnNotificationCloseEventStub(
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      mojom::ServiceWorkerEventDispatcher::
          DispatchNotificationCloseEventCallback callback);
  void OnPushEventStub(
      const PushEventPayload& payload,
      mojom::ServiceWorkerEventDispatcher::DispatchPushEventCallback callback);
  void OnPaymentRequestEventStub(
      payments::mojom::PaymentRequestEventDataPtr data,
      payments::mojom::PaymentAppResponseCallbackPtr response_callback,
      mojom::ServiceWorkerEventDispatcher::DispatchPaymentRequestEventCallback
          callback);

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<MockRenderProcessHost> render_process_host_;
  std::unique_ptr<MockRenderProcessHost> new_render_process_host_;

  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;

  IPC::TestSink sink_;

  std::vector<std::unique_ptr<MockEmbeddedWorkerInstanceClient>>
      mock_instance_clients_;
  size_t mock_instance_clients_next_index_;

  int next_thread_id_;
  int mock_render_process_id_;
  int new_mock_render_process_id_;

  std::map<int /* process_id */, scoped_refptr<ServiceWorkerDispatcherHost>>
      dispatcher_hosts_;

  std::map<int, int64_t> embedded_worker_id_service_worker_version_id_map_;
  std::map<int /* thread_id */, int /* embedded_worker_id */>
      thread_id_embedded_worker_id_map_;

  std::map<
      int /* embedded_worker_id */,
      mojom::EmbeddedWorkerInstanceHostAssociatedPtr /* instance_host_ptr */>
      embedded_worker_id_instance_host_ptr_map_;
  std::map<int /* embedded_worker_id */, ServiceWorkerRemoteProviderEndpoint>
      embedded_worker_id_remote_provider_map_;

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
