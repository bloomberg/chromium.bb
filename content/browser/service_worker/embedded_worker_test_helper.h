// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/service_worker/fake_embedded_worker_instance_client.h"
#include "content/browser/service_worker/fake_service_worker.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/url_loader_factory_getter.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/http/http_response_info.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/service_worker/embedded_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_installed_scripts_manager.mojom.h"
#include "url/gurl.h"

namespace blink {
struct PlatformNotificationData;
}

namespace content {

class EmbeddedWorkerRegistry;
class MockRenderProcessHost;
class FakeServiceWorker;
class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;
class TestBrowserContext;

// In-Process EmbeddedWorker test helper.
//
// Usage: create an instance of this class to test browser-side embedded worker
// code without creating a child process. This class will create a
// ServiceWorkerContextWrapper and ServiceWorkerContextCore for you.
//
// By default, this class uses FakeEmbeddedWorkerInstanceClient which notifies
// back success for StartWorker and StopWorker requests. It also uses
// FakeServiceWorker which returns success for event messages (e.g.
// InstallEvent, FetchEvent).
//
// Alternatively consumers can use subclasses of the Fake* classes
// to add their own logic/verification code.
//
// Example:
//
//  class MyClient : public FakeEmbeddedWorkerInstanceClient {
//    void StartWorker(...) override {
//      // Do custom stuff.
//      LOG(INFO) << "in start worker!";
//    }
//  };
//  class MyServiceWorker : public FakeServiceWorker {
//    void DispatchFetchEvent(...) override {
//      // Do custom stuff.
//      LOG(INFO) << "in fetch event!";
//    }
//  };
//
//  // Set up the fakes.
//  helper->AddPendingInstanceClient(std::make_unique<MyClient>());
//  helper->AddPendingServiceWorker(std::make_unique<MyServiceWorker>());
//
//  // Run code that starts a worker.
//  StartWorker();  // "in start worker!"
//
//  // Run code that dispatches a fetch event.
//  Navigate();  // "in fetch event!"
//
// See embedded_worker_instance_unittest.cc for more example usages.
class EmbeddedWorkerTestHelper {
 public:
  enum class Event { Install, Activate };

  // If |user_data_directory| is empty, the context makes storage stuff in
  // memory.
  explicit EmbeddedWorkerTestHelper(const base::FilePath& user_data_directory);
  virtual ~EmbeddedWorkerTestHelper();

  std::vector<Event>* dispatched_events() { return &events_; }

  ServiceWorkerContextCore* context();
  ServiceWorkerContextWrapper* context_wrapper() { return wrapper_.get(); }
  void ShutdownContext();

  int GetNextThreadId() { return next_thread_id_++; }

  int mock_render_process_id() const { return mock_render_process_id_; }
  MockRenderProcessHost* mock_render_process_host() {
    return render_process_host_.get();
  }

  // Only used for tests that force creating a new render process.
  int new_render_process_id() const { return new_mock_render_process_id_; }

  TestBrowserContext* browser_context() { return browser_context_.get(); }

  base::WeakPtr<EmbeddedWorkerTestHelper> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  static net::HttpResponseInfo CreateHttpResponseInfo();

  URLLoaderFactoryGetter* url_loader_factory_getter() {
    return url_loader_factory_getter_.get();
  }

  // Overrides the network URLLoaderFactory for subsequent requests. Passing a
  // null pointer will restore the default behavior.
  void SetNetworkFactory(network::mojom::URLLoaderFactory* factory);

  // Adds the given client to the pending queue. The next time this helper
  // receives a blink::mojom::EmbeddedWorkerInstanceClientRequest request (i.e.,
  // on the next start worker attempt), it uses the first client from this
  // queue.
  void AddPendingInstanceClient(
      std::unique_ptr<FakeEmbeddedWorkerInstanceClient> instance_client);

  // Adds the given service worker to the pending queue. The next time this
  // helper receives a blink::mojom::ServiceWorkerRequest request (i.e., on the
  // next start worker attempt), it uses the first service worker from this
  // queue.
  void AddPendingServiceWorker(
      std::unique_ptr<FakeServiceWorker> service_worker);

  // A convenience method useful for keeping a pointer to a
  // FakeEmbeddedWorkerInstanceClient after it's added. Equivalent to:
  //   auto client_to_pass = std::make_unique<MockType>(args);
  //   auto* client = client.get();
  //   AddPendingInstanceClient(std::move(client_to_pass));
  template <typename MockType, typename... Args>
  MockType* AddNewPendingInstanceClient(Args&&... args);
  // Same for FakeServiceWorker.
  template <typename MockType, typename... Args>
  MockType* AddNewPendingServiceWorker(Args&&... args);

  /////////////////////////////////////////////////////////////////////////////
  // The following are exposed to public so the fake embedded worker and service
  // worker implementations and their subclasses can call them.
  //
  // Called when |request| is received. It takes the object from a previous
  // AddPending*() call if any and calls Create*() otherwise.
  void OnInstanceClientRequest(
      blink::mojom::EmbeddedWorkerInstanceClientRequest request);
  void OnServiceWorkerRequest(blink::mojom::ServiceWorkerRequest request);

  // Called by the fakes to destroy themselves.
  void RemoveInstanceClient(FakeEmbeddedWorkerInstanceClient* instance_client);
  void RemoveServiceWorker(FakeServiceWorker* service_worker);

  // Writes a dummy script into the given service worker's
  // ServiceWorkerScriptCacheMap. |callback| is called when done.
  virtual void PopulateScriptCacheMap(int64_t service_worker_version_id,
                                      base::OnceClosure callback);
  /////////////////////////////////////////////////////////////////////////////

 protected:
  // TODO(falken): Remove these and use FakeServiceWorker instead.
  virtual void OnActivateEvent(
      blink::mojom::ServiceWorker::DispatchActivateEventCallback callback);
  virtual void OnBackgroundFetchAbortEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      blink::mojom::ServiceWorker::DispatchBackgroundFetchAbortEventCallback
          callback);
  virtual void OnBackgroundFetchClickEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      blink::mojom::ServiceWorker::DispatchBackgroundFetchClickEventCallback
          callback);
  virtual void OnBackgroundFetchFailEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      blink::mojom::ServiceWorker::DispatchBackgroundFetchFailEventCallback
          callback);
  virtual void OnBackgroundFetchSuccessEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      blink::mojom::ServiceWorker::DispatchBackgroundFetchSuccessEventCallback
          callback);
  virtual void OnCookieChangeEvent(
      const net::CanonicalCookie& cookie,
      ::network::mojom::CookieChangeCause cause,
      blink::mojom::ServiceWorker::DispatchCookieChangeEventCallback callback);
  virtual void OnExtendableMessageEvent(
      blink::mojom::ExtendableMessageEventPtr event,
      blink::mojom::ServiceWorker::DispatchExtendableMessageEventCallback
          callback);
  virtual void OnInstallEvent(
      blink::mojom::ServiceWorker::DispatchInstallEventCallback callback);
  virtual void OnFetchEvent(
      int embedded_worker_id,
      blink::mojom::FetchAPIRequestPtr request,
      blink::mojom::FetchEventPreloadHandlePtr preload_handle,
      blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      blink::mojom::ServiceWorker::DispatchFetchEventCallback finish_callback);
  virtual void OnNotificationClickEvent(
      const std::string& notification_id,
      const blink::PlatformNotificationData& notification_data,
      int action_index,
      const base::Optional<base::string16>& reply,
      blink::mojom::ServiceWorker::DispatchNotificationClickEventCallback
          callback);
  virtual void OnNotificationCloseEvent(
      const std::string& notification_id,
      const blink::PlatformNotificationData& notification_data,
      blink::mojom::ServiceWorker::DispatchNotificationCloseEventCallback
          callback);
  virtual void OnPushEvent(
      base::Optional<std::string> payload,
      blink::mojom::ServiceWorker::DispatchPushEventCallback callback);
  virtual void OnAbortPaymentEvent(
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      blink::mojom::ServiceWorker::DispatchAbortPaymentEventCallback callback);
  virtual void OnCanMakePaymentEvent(
      payments::mojom::CanMakePaymentEventDataPtr data,
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      blink::mojom::ServiceWorker::DispatchCanMakePaymentEventCallback
          callback);
  virtual void OnPaymentRequestEvent(
      payments::mojom::PaymentRequestEventDataPtr data,
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      blink::mojom::ServiceWorker::DispatchPaymentRequestEventCallback
          callback);
  virtual void OnSetIdleTimerDelayToZero(int embedded_worker_id);

  EmbeddedWorkerRegistry* registry();

  blink::mojom::ServiceWorkerHost* GetServiceWorkerHost(
      int embedded_worker_id) {
    return embedded_worker_id_host_map_[embedded_worker_id].get();
  }

  blink::mojom::EmbeddedWorkerInstanceHostProxy* GetEmbeddedWorkerInstanceHost(
      int embedded_worker_id) {
    return embedded_worker_id_instance_host_ptr_map_[embedded_worker_id].get();
  }

  // Subclasses can override these to change the default fakes. This saves tests
  // from calling AddPending*() for each start worker attempt.
  virtual std::unique_ptr<FakeEmbeddedWorkerInstanceClient>
  CreateInstanceClient();
  virtual std::unique_ptr<FakeServiceWorker> CreateServiceWorker();

 private:
  friend FakeServiceWorker;
  class MockNetworkURLLoaderFactory;
  class MockRendererInterface;

  // TODO(falken): Remove these and use FakeServiceWorker instead.
  void OnActivateEventStub(
      blink::mojom::ServiceWorker::DispatchActivateEventCallback callback);
  void OnBackgroundFetchAbortEventStub(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      blink::mojom::ServiceWorker::DispatchBackgroundFetchAbortEventCallback
          callback);
  void OnBackgroundFetchClickEventStub(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      blink::mojom::ServiceWorker::DispatchBackgroundFetchClickEventCallback
          callback);
  void OnBackgroundFetchFailEventStub(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      blink::mojom::ServiceWorker::DispatchBackgroundFetchFailEventCallback
          callback);
  void OnBackgroundFetchSuccessEventStub(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      blink::mojom::ServiceWorker::DispatchBackgroundFetchSuccessEventCallback
          callback);
  void OnCookieChangeEventStub(
      const net::CanonicalCookie& cookie,
      ::network::mojom::CookieChangeCause cause,
      blink::mojom::ServiceWorker::DispatchCookieChangeEventCallback callback);
  void OnExtendableMessageEventStub(
      blink::mojom::ExtendableMessageEventPtr event,
      blink::mojom::ServiceWorker::DispatchExtendableMessageEventCallback
          callback);
  void OnInstallEventStub(
      blink::mojom::ServiceWorker::DispatchInstallEventCallback callback);
  void OnFetchEventStub(
      int embedded_worker_id,
      blink::mojom::FetchAPIRequestPtr request,
      blink::mojom::FetchEventPreloadHandlePtr preload_handle,
      blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      blink::mojom::ServiceWorker::DispatchFetchEventCallback finish_callback);
  void OnNotificationClickEventStub(
      const std::string& notification_id,
      const blink::PlatformNotificationData& notification_data,
      int action_index,
      const base::Optional<base::string16>& reply,
      blink::mojom::ServiceWorker::DispatchNotificationClickEventCallback
          callback);
  void OnNotificationCloseEventStub(
      const std::string& notification_id,
      const blink::PlatformNotificationData& notification_data,
      blink::mojom::ServiceWorker::DispatchNotificationCloseEventCallback
          callback);
  void OnPushEventStub(
      base::Optional<std::string> payload,
      blink::mojom::ServiceWorker::DispatchPushEventCallback callback);
  void OnAbortPaymentEventStub(
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      blink::mojom::ServiceWorker::DispatchAbortPaymentEventCallback callback);
  void OnCanMakePaymentEventStub(
      payments::mojom::CanMakePaymentEventDataPtr data,
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      blink::mojom::ServiceWorker::DispatchCanMakePaymentEventCallback
          callback);
  void OnPaymentRequestEventStub(
      payments::mojom::PaymentRequestEventDataPtr data,
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      blink::mojom::ServiceWorker::DispatchPaymentRequestEventCallback
          callback);

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<MockRenderProcessHost> render_process_host_;
  std::unique_ptr<MockRenderProcessHost> new_render_process_host_;

  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;

  std::unique_ptr<MockRendererInterface> mock_renderer_interface_;

  base::queue<std::unique_ptr<FakeEmbeddedWorkerInstanceClient>>
      pending_embedded_worker_instance_clients_;
  base::flat_set<std::unique_ptr<FakeEmbeddedWorkerInstanceClient>,
                 base::UniquePtrComparator>
      instance_clients_;

  base::queue<std::unique_ptr<FakeServiceWorker>> pending_service_workers_;
  base::flat_set<std::unique_ptr<FakeServiceWorker>, base::UniquePtrComparator>
      service_workers_;

  int next_thread_id_;
  int mock_render_process_id_;
  int new_mock_render_process_id_;

  std::map<int, int64_t> embedded_worker_id_service_worker_version_id_map_;

  std::map<int /* embedded_worker_id */,
           blink::mojom::
               EmbeddedWorkerInstanceHostAssociatedPtr /* instance_host_ptr */>
      embedded_worker_id_instance_host_ptr_map_;
  std::map<int /* embedded_worker_id */, ServiceWorkerRemoteProviderEndpoint>
      embedded_worker_id_remote_provider_map_;
  std::map<int /* embedded_worker_id */,
           blink::mojom::ServiceWorkerInstalledScriptsInfoPtr>
      embedded_worker_id_installed_scripts_info_map_;
  std::map<
      int /* embedded_worker_id */,
      blink::mojom::ServiceWorkerHostAssociatedPtr /* service_worker_host */>
      embedded_worker_id_host_map_;
  std::map<int /* embedded_worker_id */,
           blink::mojom::
               ServiceWorkerRegistrationObjectInfoPtr /* registration_info */>
      embedded_worker_id_registration_info_map_;

  std::vector<Event> events_;
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter_;
  std::unique_ptr<MockNetworkURLLoaderFactory> default_network_loader_factory_;

  base::WeakPtrFactory<EmbeddedWorkerTestHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerTestHelper);
};

template <typename MockType, typename... Args>
MockType* EmbeddedWorkerTestHelper::AddNewPendingInstanceClient(
    Args&&... args) {
  std::unique_ptr<MockType> mock =
      std::make_unique<MockType>(std::forward<Args>(args)...);
  MockType* mock_rawptr = mock.get();
  AddPendingInstanceClient(std::move(mock));
  return mock_rawptr;
}

template <typename MockType, typename... Args>
MockType* EmbeddedWorkerTestHelper::AddNewPendingServiceWorker(Args&&... args) {
  std::unique_ptr<MockType> mock =
      std::make_unique<MockType>(std::forward<Args>(args)...);
  MockType* mock_rawptr = mock.get();
  AddPendingServiceWorker(std::move(mock));
  return mock_rawptr;
}

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_
