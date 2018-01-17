// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/WebKit/common/service_worker/service_worker_error_type.mojom.h"
#include "third_party/WebKit/common/service_worker/service_worker_registration.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebServiceWorkerRegistrationProxy;
}

namespace content {

class WebServiceWorkerImpl;
class ServiceWorkerProviderContext;

// WebServiceWorkerRegistrationImpl corresponds to one ServiceWorkerRegistration
// object in JavaScript. It is owned by
// content::ServiceWorkerRegistrationObjectHost in the browser process, but also
// is refcounted by WebServiceWorkerRegistration::Handles in the renderer
// process. See the detailed lifecycle explanation below.
//
// A WebServiceWorkerRegistrationImpl is created when the browser process sends
// the first ServiceWorkerRegistrationObjectInfo to the renderer process that
// describes the desired JavaScript object. The instance is created and takes
// ownership of the object info. The object info has a Mojo connection to a
// ServiceWorkerRegistrationObjectHost in the browser process
// ((|this->info_.host_ptr_info|) . In addition, The
// WebServiceWorkerRegistrationImpl itself is connected with the
// ServiceWorkerRegistrationObjectHost (|this->binding_|). Creation always
// happens in order to create a ServiceWorkerRegistration JavaScript object in
// Blink. The instance is shared with Blink via
// WebServiceWorkerRegistration::Handle. As long as a handle is alive in Blink,
// this instance should not die.
//
// During the lifetime of WebServiceWorkerRegistrationImpl, multiple
// WebServiceWorkerRegistration::Handles may be created and held by Blink. If
// the browser process sends another ServiceWorkerRegistrationObjectInfo to the
// renderer process for this same JavaScript object, the renderer reuses the
// existing WebServiceWorkerRegistrationImpl instance and creates a new
// WebServiceWorkerRegistration::Handle to share with Blink.
//
// If all WebServiceWorkerRegistration::Handles are destroyed, the
// WebServiceWorkerRegistrationImpl clears |info_|, which informs the
// ServiceWorkerRegistrationObjectHost in the browser process that this instance
// is ready to be destroyed. If there was no ServiceWorkerRegistrationObjectInfo
// inflight, the browser process destroys the Mojo connection to this instance,
// which finally destroys it.
//
// Another destruction scenario is that the browser process destroys the
// ServiceWorkerRegistrationObject Mojo connection while some
// WebServiceWorkerRegistration::Handles are still held by Blink. In such a case
// this instance will finally be destroyed after all Blink destroys all the
// WebServiceWorkerRegistration::Handles.
class CONTENT_EXPORT WebServiceWorkerRegistrationImpl
    : public blink::mojom::ServiceWorkerRegistrationObject,
      public blink::WebServiceWorkerRegistration,
      public base::RefCounted<WebServiceWorkerRegistrationImpl,
                              WebServiceWorkerRegistrationImpl> {
 public:
  // |io_task_runner| is used to bind |host_for_global_scope_| and |binding_|
  // for service worker execution context, as both of
  // ServiceWorkerRegistrationObjectHost and ServiceWorkerRegistrationObject are
  // Channel-associated interfaces and need to be bound on either the main or IO
  // thread.
  static scoped_refptr<WebServiceWorkerRegistrationImpl>
  CreateForServiceWorkerGlobalScope(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  static scoped_refptr<WebServiceWorkerRegistrationImpl>
  CreateForServiceWorkerClient(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
      base::WeakPtr<ServiceWorkerProviderContext> provider_context);

  void AttachForServiceWorkerClient(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info);

  void SetInstalling(const scoped_refptr<WebServiceWorkerImpl>& service_worker);
  void SetWaiting(const scoped_refptr<WebServiceWorkerImpl>& service_worker);
  void SetActive(const scoped_refptr<WebServiceWorkerImpl>& service_worker);

  // blink::WebServiceWorkerRegistration overrides.
  void SetProxy(blink::WebServiceWorkerRegistrationProxy* proxy) override;
  blink::WebServiceWorkerRegistrationProxy* Proxy() override;
  blink::WebURL Scope() const override;
  void Update(
      std::unique_ptr<WebServiceWorkerUpdateCallbacks> callbacks) override;
  void Unregister(std::unique_ptr<WebServiceWorkerUnregistrationCallbacks>
                      callbacks) override;
  void EnableNavigationPreload(
      bool enable,
      std::unique_ptr<WebEnableNavigationPreloadCallbacks> callbacks) override;
  void GetNavigationPreloadState(
      std::unique_ptr<WebGetNavigationPreloadStateCallbacks> callbacks)
      override;
  void SetNavigationPreloadHeader(
      const blink::WebString& value,
      std::unique_ptr<WebSetNavigationPreloadHeaderCallbacks> callbacks)
      override;
  int64_t RegistrationId() const override;

  // Creates blink::WebServiceWorkerRegistration::Handle object that owns a
  // reference to the given WebServiceWorkerRegistrationImpl object.
  static std::unique_ptr<blink::WebServiceWorkerRegistration::Handle>
  CreateHandle(scoped_refptr<WebServiceWorkerRegistrationImpl> registration);

 private:
  class WebCallbacksHolder;
  friend class base::RefCounted<WebServiceWorkerRegistrationImpl,
                                WebServiceWorkerRegistrationImpl>;
  WebServiceWorkerRegistrationImpl(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
      base::WeakPtr<ServiceWorkerProviderContext> provider_context);
  ~WebServiceWorkerRegistrationImpl() override;

  // Implements blink::mojom::ServiceWorkerRegistrationObject.
  void SetVersionAttributes(
      int changed_mask,
      blink::mojom::ServiceWorkerObjectInfoPtr installing,
      blink::mojom::ServiceWorkerObjectInfoPtr waiting,
      blink::mojom::ServiceWorkerObjectInfoPtr active) override;
  void UpdateFound() override;

  using ResponseCallback =
      base::OnceCallback<void(blink::mojom::ServiceWorkerErrorType,
                              const base::Optional<std::string>&)>;
  // |callback| owns a Web*Callbacks instance which constrains itself to be
  // destroyed on the same thread that created it (the worker thread).
  //
  // But our |host_for_global_scope_| is a Mojo thread safe ptr, which is used
  // to make Mojo calls on the worker thread but its underlying Mojo connection
  // is bound on the IO thread. Currently when we call |host_for_global_scope_|
  // methods, we're moving |callback| along the way from the worker thread to
  // the IO thread, then waiting until |callback| is posted back to the worker
  // thread. However, the post back operation will fail if the worker thread has
  // already been destroyed for some reasons, in such a case the poster on the
  // IO thread has to destroy |callback| there, then causing a crash.
  //
  // To solve the above problem, this function creates a wrapper response
  // callback adapting to run |callback|, since the Web*Callbacks are stored
  // separately outside it, this wrapper callback is thread-safe and can be
  // destroyed on any thread, thus, we can pass this wrapper callback to
  // |host_for_global_scope_| which can move this wrapper between the worker
  // thread and IO thread freely.
  //
  // TODO(leonhsl): Once we can avoid using Mojo thread safe ptr for
  // |host_for_global_scope_|, we can eliminate this wrapping mechanism.
  ResponseCallback WrapResponseCallback(ResponseCallback callback);
  blink::mojom::ServiceWorkerRegistrationObjectHost::
      GetNavigationPreloadStateCallback
      WrapWebGetNavigationPreloadStateCallbacks(
          std::unique_ptr<WebGetNavigationPreloadStateCallbacks> callbacks);

  // RefCounted traits implementation, rather than delete |impl| directly, calls
  // |impl->DetachAndMaybeDestroy()| to notify that the last reference to it has
  // gone away.
  static void Destruct(const WebServiceWorkerRegistrationImpl* impl);

  // Enumeration of the possible |state_| during lifetime of this class.
  // -     |kInitial| --> |kAttachedAndBound|
  //   |this| is initialized with |kInitial| state by the ctor then it is
  //   set to |kAttachedAndBound| state soon in the factory methods
  //   (CreateForServiceWorkerGlobalScope() or CreateForServiceWorkerClient()).
  //   From the beginning |this| is referenced by
  //   blink::WebServiceWorkerRegistration::Handle impl and the |binding_| Mojo
  //   connection has been established.
  // -     |kAttachedAndBound| --> |kDetached|
  //   When all references to |this| have been released by Blink,
  //   DetachAndMaybeDestroy() will be triggered to change |state_| from
  //   |kAttachedAndBound| to |kDetached|.
  // -     |kAttachedAndBound| --> |kUnbound|
  //   When |binding_| Mojo connection gets broken, OnConnectionError() will be
  //   triggered to change |state_| from |kAttachedAndBound| to |kUnbound|.
  // -     {|kUnbound|, |kDetached|} --> |kDead|
  //   But if DetachAndMaybeDestroy() saw that |state_| is already |kUnbound| or
  //   OnConnectionError() saw that |state_| is already |kDetached|, they will
  //   just set |state_| to |kDead| and delete |this| immediately.
  // -     |kDetached| --> |kAttachedAndBound|
  //   When |this| is in |kDetached| state, if an inflight
  //   ServiceWorkerRegistrationObjectInfo for the same JavaScript registration
  //   object arrived, |this| is reused to be provided to Blink. In such a case
  //   AttachForServiceWorkerClient() sets |state_| to |kAttachedAndBound|.
  enum class LifecycleState {
    kInitial,
    kAttachedAndBound,
    kUnbound,
    kDetached,
    kDead
  };

  enum QueuedTaskType {
    INSTALLING,
    WAITING,
    ACTIVE,
    UPDATE_FOUND,
  };

  struct QueuedTask {
    QueuedTask(QueuedTaskType type,
               const scoped_refptr<WebServiceWorkerImpl>& worker);
    QueuedTask(const QueuedTask& other);
    ~QueuedTask();
    QueuedTaskType type;
    scoped_refptr<WebServiceWorkerImpl> worker;
  };

  void RunQueuedTasks();
  blink::mojom::ServiceWorkerRegistrationObjectHost*
  GetRegistrationObjectHost();
  void Attach(blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info);
  void DetachAndMaybeDestroy();
  void BindRequest(
      blink::mojom::ServiceWorkerRegistrationObjectAssociatedRequest request);
  void OnConnectionError();

  // |registration_id_| is the id of the corresponding
  // content::ServiceWorkerRegistration in the browser process.
  const int64_t registration_id_;
  // |info_| is initialized by the contructor with |info| passed from the remote
  // content::ServiceWorkerRegistrationObjectHost just created in the browser
  // process. It will be reset to nullptr by DetachAndMaybeDestroy() when
  // there is no any blink::WebServiceWorkerRegistration::Handle referencing
  // |this|. After that if another Mojo connection from the same remote
  // content::ServiceWorkerRegistrationObjectHost is passed here again (e.g.
  // WebServiceWorkerProviderImpl::OnDidGetRegistration()), |info_| will be set
  // to the valid value again by Attach().
  // |info_->host_ptr_info| is taken/bound by |host_for_global_scope_| or
  // |host_for_client_| which holds the Mojo connection caller end point
  // retaining an reference to the remote
  // content::ServiceWorkerRegistrationObjectHost to control its lifetime.
  // |info_->request| is bound on |binding_|.
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info_;
  blink::WebServiceWorkerRegistrationProxy* proxy_;

  // Either |host_for_global_scope_| or |host_for_client_| is non-null.
  //
  // |host_for_global_scope_| is for service worker execution contexts. It is
  // used on the worker thread but bound on the IO thread, because it's a
  // channel- associated interface which can be bound only on the main or IO
  // thread.
  // TODO(leonhsl): Once we can detach this interface out from the legacy IPC
  // channel-associated interfaces world, we should bind it always on the worker
  // thread on which |this| lives.
  // Although it is a scoped_refptr, the only one owner is |this|.
  scoped_refptr<
      blink::mojom::ThreadSafeServiceWorkerRegistrationObjectHostAssociatedPtr>
      host_for_global_scope_;
  // |host_for_client_| is for service worker clients (document, shared worker).
  // It is bound and used on the main thread.
  blink::mojom::ServiceWorkerRegistrationObjectHostAssociatedPtr
      host_for_client_;

  // |binding_| keeps the Mojo binding to serve its other Mojo endpoint (i.e.
  // the caller end) held by the content::ServiceWorkerRegistrationObjectHost in
  // the browser process, is bound with |info_->request| by BindRequest()
  // function. This also controls lifetime of |this|, its connection error
  // handler will delete |this|. It is bound on the main thread for service
  // worker clients (document, shared worker). It is bound on the IO thread for
  // service worker execution contexts, but always uses PostTask to handle
  // received messages actually on the worker thread, because it's a
  // channel-associated interface which can be bound only on the main or IO
  // thread.
  // TODO(leonhsl): Once we can detach this interface out from the legacy IPC
  // channel-associated interfaces world, for service worker execution context
  // we should bind it always on the worker thread on which |this| lives.
  mojo::AssociatedBinding<blink::mojom::ServiceWorkerRegistrationObject>
      binding_;
  scoped_refptr<base::SingleThreadTaskRunner> creation_task_runner_;
  LifecycleState state_;

  std::vector<QueuedTask> queued_tasks_;

  // Owns all WebServiceWorkerXXXCallbacks provided from Blink when calling
  // ServiceWorkerRegistrationObjectHost interface methods. Please see comments
  // of WrapResponseCallback() for details.
  std::unique_ptr<WebCallbacksHolder> web_callbacks_holder_;

  // For service worker client contexts, |this| is tracked (not owned) in
  // |provider_context_for_client_->controllee_state_->registrations_|.
  // For service worker execution contexts, |provider_context_for_client_| is
  // null.
  base::WeakPtr<ServiceWorkerProviderContext> provider_context_for_client_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerRegistrationImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_
