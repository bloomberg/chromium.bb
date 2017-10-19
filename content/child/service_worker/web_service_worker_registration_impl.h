// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_
#define CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_error_type.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebServiceWorkerRegistrationProxy;
}

namespace content {

class WebServiceWorkerImpl;

// WebServiceWorkerRegistrationImpl corresponds to one ServiceWorkerRegistration
// object in JavaScript. It is owned by content::ServiceWorkerRegistrationHandle
// in the browser process, but also is refcounted by
// WebServiceWorkerRegistration::Handles in the renderer process. See the
// detailed lifecycle explanation below.
//
// A WebServiceWorkerRegistrationImpl is created when the browser process sends
// the first ServiceWorkerRegistrationObjectInfo to the renderer process that
// describes the desired JavaScript object. The instance is created and takes
// ownership of the object info. The object info has a Mojo connection to a
// ServiceWorkerRegistrationHandle in the browser process
// ((|this->info_.host_ptr_info|) . In addition, The
// WebServiceWorkerRegistrationImpl itself is connected with the
// ServiceWorkerRegistrationHandle (|this->binding_|). Creation always happens
// in order to create a ServiceWorkerRegistration JavaScript object in Blink.
// The instance is shared with Blink via WebServiceWorkerRegistration::Handle.
// As long as a handle is alive in Blink, this instance should not die.
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
// ServiceWorkerRegistrationHandle in the browser process that this instance is
// ready to be destroyed. If there was no ServiceWorkerRegistrationObjectInfo
// inflight, the browser process destroys the Mojo connection to this instance,
// which finally destroys it.
class CONTENT_EXPORT WebServiceWorkerRegistrationImpl
    : public blink::mojom::ServiceWorkerRegistrationObject,
      public blink::WebServiceWorkerRegistration,
      public base::RefCounted<WebServiceWorkerRegistrationImpl,
                              WebServiceWorkerRegistrationImpl> {
 public:
  // |io_task_runner| is used to bind |host_for_global_scope_|, as it's a
  // Channel-associated interface and needs to be bound on either the main or IO
  // thread.
  static scoped_refptr<WebServiceWorkerRegistrationImpl>
  CreateForServiceWorkerGlobalScope(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  static scoped_refptr<WebServiceWorkerRegistrationImpl>
  CreateForServiceWorkerClient(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info);

  void AttachForServiceWorkerGlobalScope(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  void AttachForServiceWorkerClient(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info);

  void SetInstalling(const scoped_refptr<WebServiceWorkerImpl>& service_worker);
  void SetWaiting(const scoped_refptr<WebServiceWorkerImpl>& service_worker);
  void SetActive(const scoped_refptr<WebServiceWorkerImpl>& service_worker);

  void OnUpdateFound();

  // blink::WebServiceWorkerRegistration overrides.
  void SetProxy(blink::WebServiceWorkerRegistrationProxy* proxy) override;
  blink::WebServiceWorkerRegistrationProxy* Proxy() override;
  blink::WebURL Scope() const override;
  void Update(
      blink::WebServiceWorkerProvider* provider,
      std::unique_ptr<WebServiceWorkerUpdateCallbacks> callbacks) override;
  void Unregister(blink::WebServiceWorkerProvider* provider,
                  std::unique_ptr<WebServiceWorkerUnregistrationCallbacks>
                      callbacks) override;
  void EnableNavigationPreload(
      bool enable,
      blink::WebServiceWorkerProvider* provider,
      std::unique_ptr<WebEnableNavigationPreloadCallbacks> callbacks) override;
  void GetNavigationPreloadState(
      blink::WebServiceWorkerProvider* provider,
      std::unique_ptr<WebGetNavigationPreloadStateCallbacks> callbacks)
      override;
  void SetNavigationPreloadHeader(
      const blink::WebString& value,
      blink::WebServiceWorkerProvider* provider,
      std::unique_ptr<WebSetNavigationPreloadHeaderCallbacks> callbacks)
      override;
  int64_t RegistrationId() const override;

  // Creates blink::WebServiceWorkerRegistration::Handle object that owns a
  // reference to the given WebServiceWorkerRegistrationImpl object.
  static std::unique_ptr<blink::WebServiceWorkerRegistration::Handle>
  CreateHandle(scoped_refptr<WebServiceWorkerRegistrationImpl> registration);

 private:
  friend class base::RefCounted<WebServiceWorkerRegistrationImpl,
                                WebServiceWorkerRegistrationImpl>;
  explicit WebServiceWorkerRegistrationImpl(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info);
  ~WebServiceWorkerRegistrationImpl() override;

  // RefCounted traits implementation, rather than delete |impl| directly, calls
  // |impl->DetachAndMaybeDestroy()| to notify that the last reference to it has
  // gone away.
  static void Destruct(const WebServiceWorkerRegistrationImpl* impl);

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
  void OnConnectionError();

  void OnUpdated(std::unique_ptr<WebServiceWorkerUpdateCallbacks> callbacks,
                 blink::mojom::ServiceWorkerErrorType error,
                 const base::Optional<std::string>& error_msg);

  // |handle_id_| is the key to map with remote
  // content::ServiceWorkerRegistrationHandle.
  const int handle_id_;
  // |info_| is initialized by the contructor with |info| passed from the remote
  // content::ServiceWorkerRegistrationHandle just created in the browser
  // process. It will be reset to nullptr by DetachAndMaybeDestroy() when
  // there is no any blink::WebServiceWorkerRegistration::Handle referencing
  // |this|. After that if another Mojo connection from the same remote
  // content::ServiceWorkerRegistrationHandle is passed here again (e.g.
  // WebServiceWorkerProviderImpl::OnDidGetRegistration()), |info_| will be set
  // to the valid value again by Attach().
  // |info_->host_ptr_info| is taken/bound by |host_for_global_scope_| or
  // |host_for_client_| which holds the Mojo connection caller end point
  // retaining an reference to the remote
  // content::ServiceWorkerRegistrationHandle to control its lifetime.
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
  // the caller end) held by the content::ServiceWorkerRegistrationHandle in the
  // browser process, is bound with |info_->request| by the constructor.
  // This also controls lifetime of |this|, its connection error handler will
  // delete |this|.
  mojo::AssociatedBinding<blink::mojom::ServiceWorkerRegistrationObject>
      binding_;

  std::vector<QueuedTask> queued_tasks_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerRegistrationImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_
