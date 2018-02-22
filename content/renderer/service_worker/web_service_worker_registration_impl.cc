// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/web_service_worker_registration_impl.h"

#include <map>
#include <utility>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/child_process.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/renderer/service_worker/service_worker_dispatcher.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "content/renderer/service_worker/web_service_worker_impl.h"
#include "content/renderer/service_worker/web_service_worker_provider_impl.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebNavigationPreloadState.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistrationProxy.h"

namespace content {

namespace {

class ServiceWorkerRegistrationHandleImpl
    : public blink::WebServiceWorkerRegistration::Handle {
 public:
  explicit ServiceWorkerRegistrationHandleImpl(
      scoped_refptr<WebServiceWorkerRegistrationImpl> registration)
      : registration_(std::move(registration)) {}
  ~ServiceWorkerRegistrationHandleImpl() override {}

  blink::WebServiceWorkerRegistration* Registration() override {
    return registration_.get();
  }

 private:
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistrationHandleImpl);
};

}  // namespace

class WebServiceWorkerRegistrationImpl::WebCallbacksHolder {
 public:
  WebCallbacksHolder() : weak_ptr_factory_(this){};
  ~WebCallbacksHolder() = default;

  WebServiceWorkerRegistrationImpl::ResponseCallback WrapResponseCallback(
      WebServiceWorkerRegistrationImpl::ResponseCallback callback) {
    const uint64_t callback_id = next_callback_id_++;
    callback_map_[callback_id] = std::move(callback);
    return base::BindOnce(&WebCallbacksHolder::OnRegistrationObjectHostResponse,
                          weak_ptr_factory_.GetWeakPtr(), callback_id);
  }

  blink::mojom::ServiceWorkerRegistrationObjectHost::
      GetNavigationPreloadStateCallback
      WrapWebGetNavigationPreloadStateCallbacks(
          std::unique_ptr<WebGetNavigationPreloadStateCallbacks> callbacks) {
    const int web_callbacks_id =
        get_navigation_preload_state_callbacks_map_.Add(std::move(callbacks));
    return base::BindOnce(
        &WebCallbacksHolder::OnGetNavigationPreloadStateResponse,
        weak_ptr_factory_.GetWeakPtr(), web_callbacks_id);
  }

  // Gets the response callback from |callback_map_| with the key |callback_id|,
  // then runs it.
  void OnRegistrationObjectHostResponse(
      uint64_t callback_id,
      blink::mojom::ServiceWorkerErrorType error,
      const base::Optional<std::string>& error_msg) {
    auto it = callback_map_.find(callback_id);
    DCHECK(it != callback_map_.end());
    std::move(it->second).Run(error, error_msg);
    callback_map_.erase(it);
  }

  void OnGetNavigationPreloadStateResponse(
      int web_callbacks_id,
      blink::mojom::ServiceWorkerErrorType error,
      const base::Optional<std::string>& error_msg,
      blink::mojom::NavigationPreloadStatePtr state) {
    WebGetNavigationPreloadStateCallbacks* callbacks =
        get_navigation_preload_state_callbacks_map_.Lookup(web_callbacks_id);
    DCHECK(callbacks);

    if (error == blink::mojom::ServiceWorkerErrorType::kNone) {
      callbacks->OnSuccess(blink::WebNavigationPreloadState(
          state->enabled, blink::WebString::FromUTF8(state->header)));
    } else {
      DCHECK(error_msg);
      callbacks->OnError(blink::WebServiceWorkerError(
          error, blink::WebString::FromUTF8(*error_msg)));
    }
    get_navigation_preload_state_callbacks_map_.Remove(web_callbacks_id);
  }

 private:
  std::map<uint64_t, WebServiceWorkerRegistrationImpl::ResponseCallback>
      callback_map_;
  uint64_t next_callback_id_ = 0;
  // Because the response callback of GetNavigationPreloadState() has different
  // prototype with other methods of ServiceWorkerRegistrationObjectHost
  // interface, we have to create this separate map for it.
  base::IDMap<std::unique_ptr<WebGetNavigationPreloadStateCallbacks>>
      get_navigation_preload_state_callbacks_map_;

  base::WeakPtrFactory<WebCallbacksHolder> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebCallbacksHolder);
};

WebServiceWorkerRegistrationImpl::QueuedTask::QueuedTask(
    QueuedTaskType type,
    const scoped_refptr<WebServiceWorkerImpl>& worker)
    : type(type), worker(worker) {}

WebServiceWorkerRegistrationImpl::QueuedTask::QueuedTask(
    const QueuedTask& other) = default;

WebServiceWorkerRegistrationImpl::QueuedTask::~QueuedTask() {}

// static
scoped_refptr<WebServiceWorkerRegistrationImpl>
WebServiceWorkerRegistrationImpl::CreateForServiceWorkerGlobalScope(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  DCHECK(info->request.is_pending());
  scoped_refptr<WebServiceWorkerRegistrationImpl> impl =
      new WebServiceWorkerRegistrationImpl(std::move(info),
                                           nullptr /* provider_context */);
  impl->host_for_global_scope_ =
      blink::mojom::ThreadSafeServiceWorkerRegistrationObjectHostAssociatedPtr::
          Create(std::move(impl->info_->host_ptr_info), io_task_runner);
  // |impl|'s destruction needs both DetachAndMaybeDestroy() and
  // OnConnectionError() to be called (see comments at LifecycleState enum), and
  // OnConnectionError() cannot happen before BindRequest(), therefore using
  // base::Unretained() here is safe.
  io_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&WebServiceWorkerRegistrationImpl::BindRequest,
                                base::Unretained(impl.get()),
                                std::move(impl->info_->request)));
  impl->state_ = LifecycleState::kAttachedAndBound;
  impl->RefreshVersionAttributes();
  return impl;
}

// static
scoped_refptr<WebServiceWorkerRegistrationImpl>
WebServiceWorkerRegistrationImpl::CreateForServiceWorkerClient(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
    base::WeakPtr<ServiceWorkerProviderContext> provider_context) {
  DCHECK(info->request.is_pending());
  scoped_refptr<WebServiceWorkerRegistrationImpl> impl =
      new WebServiceWorkerRegistrationImpl(std::move(info),
                                           std::move(provider_context));
  impl->host_for_client_.Bind(std::move(impl->info_->host_ptr_info));
  impl->BindRequest(std::move(impl->info_->request));
  impl->state_ = LifecycleState::kAttachedAndBound;
  impl->RefreshVersionAttributes();
  return impl;
}

void WebServiceWorkerRegistrationImpl::AttachForServiceWorkerClient(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info) {
  if (state_ == LifecycleState::kAttachedAndBound) {
    // |update_via_cache| is handled specifically here as it is the only mutable
    // property when the browser process sends |info| for an existing
    // registration. The installing/waiting/active properties are changed by the
    // SetVersionAttributes method instead.
    if (info_->options && info->options) {
      info_->options->update_via_cache = info->options->update_via_cache;
    }
    return;
  }
  DCHECK_EQ(LifecycleState::kDetached, state_);
  DCHECK(!info->request.is_pending());
  Attach(std::move(info));

  DCHECK(!host_for_global_scope_);
  DCHECK(!host_for_client_);
  host_for_client_.Bind(std::move(info_->host_ptr_info));
  state_ = LifecycleState::kAttachedAndBound;
  RefreshVersionAttributes();
}

void WebServiceWorkerRegistrationImpl::SetProxy(
    blink::WebServiceWorkerRegistrationProxy* proxy) {
  DCHECK_EQ(LifecycleState::kAttachedAndBound, state_);
  DCHECK(info_);
  DCHECK(host_for_global_scope_ || host_for_client_);
  proxy_ = proxy;
  RunQueuedTasks();
}

void WebServiceWorkerRegistrationImpl::RunQueuedTasks() {
  DCHECK(proxy_);
  for (const QueuedTask& task : queued_tasks_) {
    if (task.type == INSTALLING)
      proxy_->SetInstalling(WebServiceWorkerImpl::CreateHandle(task.worker));
    else if (task.type == WAITING)
      proxy_->SetWaiting(WebServiceWorkerImpl::CreateHandle(task.worker));
    else if (task.type == ACTIVE)
      proxy_->SetActive(WebServiceWorkerImpl::CreateHandle(task.worker));
    else if (task.type == UPDATE_FOUND)
      proxy_->DispatchUpdateFoundEvent();
  }
  queued_tasks_.clear();
}

blink::mojom::ServiceWorkerRegistrationObjectHost*
WebServiceWorkerRegistrationImpl::GetRegistrationObjectHost() {
  if (host_for_client_)
    return host_for_client_.get();
  if (host_for_global_scope_)
    return host_for_global_scope_->get();
  NOTREACHED();
  return nullptr;
}

void WebServiceWorkerRegistrationImpl::Attach(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info) {
  DCHECK(!info_);
  DCHECK(info);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            info->registration_id);
  DCHECK_EQ(registration_id_, info->registration_id);
  DCHECK(info->host_ptr_info.is_valid());
  info_ = std::move(info);
}

void WebServiceWorkerRegistrationImpl::DetachAndMaybeDestroy() {
  DCHECK(creation_task_runner_->RunsTasksInCurrentSequence());
  proxy_ = nullptr;
  queued_tasks_.clear();
  host_for_client_.reset();
  host_for_global_scope_ = nullptr;
  info_ = nullptr;
  web_callbacks_holder_.reset();
  if (state_ == LifecycleState::kUnbound) {
    state_ = LifecycleState::kDead;
    delete this;
    return;
  }
  DCHECK_EQ(LifecycleState::kAttachedAndBound, state_);
  state_ = LifecycleState::kDetached;
  // We will continue in OnConnectionError() triggered by destruction of the
  // content::ServiceWorkerRegistrationObjectHost in the browser process, or
  // else in Attach*() if |this| is reused.
}

void WebServiceWorkerRegistrationImpl::BindRequest(
    blink::mojom::ServiceWorkerRegistrationObjectAssociatedRequest request) {
  DCHECK(request.is_pending());
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(
      base::BindOnce(&WebServiceWorkerRegistrationImpl::OnConnectionError,
                     base::Unretained(this)));
}

void WebServiceWorkerRegistrationImpl::OnConnectionError() {
  if (!creation_task_runner_->RunsTasksInCurrentSequence()) {
    // If this registration impl is for a service worker execution context,
    // |this| lives on the worker thread but |binding_| is bound on the IO
    // thread due to limitations of channel-associated interfaces. Close
    // |binding_| here since this is the thread it was bound on, then hop to the
    // worker thread to handle lifetime of |this|. In the case of a service
    // worker client, both |this| and |binding_| live on the main thread, so the
    // binding can be closed normally during destruction.
    binding_.Close();
    creation_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebServiceWorkerRegistrationImpl::OnConnectionError,
                       base::Unretained(this)));
    return;
  }
  if (state_ == LifecycleState::kDetached) {
    state_ = LifecycleState::kDead;
    delete this;
    return;
  }
  DCHECK_EQ(LifecycleState::kAttachedAndBound, state_);
  state_ = LifecycleState::kUnbound;
  // We will continue in DetachAndMaybeDestroy() when all references of |this|
  // have been released by Blink.
}

blink::WebServiceWorkerRegistrationProxy*
WebServiceWorkerRegistrationImpl::Proxy() {
  return proxy_;
}

blink::WebURL WebServiceWorkerRegistrationImpl::Scope() const {
  return info_->options->scope;
}

blink::mojom::ServiceWorkerUpdateViaCache
WebServiceWorkerRegistrationImpl::UpdateViaCache() const {
  return info_->options->update_via_cache;
}

void WebServiceWorkerRegistrationImpl::Update(
    std::unique_ptr<WebServiceWorkerUpdateCallbacks> callbacks) {
  DCHECK(state_ == LifecycleState::kAttachedAndBound ||
         state_ == LifecycleState::kUnbound);
  GetRegistrationObjectHost()->Update(WrapResponseCallback(base::BindOnce(
      [](std::unique_ptr<WebServiceWorkerUpdateCallbacks> callbacks,
         blink::mojom::ServiceWorkerErrorType error,
         const base::Optional<std::string>& error_msg) {
        if (error != blink::mojom::ServiceWorkerErrorType::kNone) {
          DCHECK(error_msg);
          callbacks->OnError(blink::WebServiceWorkerError(
              error, blink::WebString::FromUTF8(*error_msg)));
          return;
        }

        DCHECK(!error_msg);
        callbacks->OnSuccess();
      },
      std::move(callbacks))));
}

void WebServiceWorkerRegistrationImpl::Unregister(
    std::unique_ptr<WebServiceWorkerUnregistrationCallbacks> callbacks) {
  DCHECK(state_ == LifecycleState::kAttachedAndBound ||
         state_ == LifecycleState::kUnbound);
  GetRegistrationObjectHost()->Unregister(WrapResponseCallback(base::BindOnce(
      [](std::unique_ptr<WebServiceWorkerUnregistrationCallbacks> callbacks,
         blink::mojom::ServiceWorkerErrorType error,
         const base::Optional<std::string>& error_msg) {
        if (error != blink::mojom::ServiceWorkerErrorType::kNone &&
            error != blink::mojom::ServiceWorkerErrorType::kNotFound) {
          DCHECK(error_msg);
          callbacks->OnError(blink::WebServiceWorkerError(
              error, blink::WebString::FromUTF8(*error_msg)));
          return;
        }

        callbacks->OnSuccess(error ==
                             blink::mojom::ServiceWorkerErrorType::kNone);
      },
      std::move(callbacks))));
}

void WebServiceWorkerRegistrationImpl::EnableNavigationPreload(
    bool enable,
    std::unique_ptr<WebEnableNavigationPreloadCallbacks> callbacks) {
  DCHECK(state_ == LifecycleState::kAttachedAndBound ||
         state_ == LifecycleState::kUnbound);
  GetRegistrationObjectHost()->EnableNavigationPreload(
      enable,
      WrapResponseCallback(base::BindOnce(
          [](std::unique_ptr<WebEnableNavigationPreloadCallbacks> callbacks,
             blink::mojom::ServiceWorkerErrorType error,
             const base::Optional<std::string>& error_msg) {
            if (error != blink::mojom::ServiceWorkerErrorType::kNone) {
              DCHECK(error_msg);
              callbacks->OnError(blink::WebServiceWorkerError(
                  error, blink::WebString::FromUTF8(*error_msg)));
              return;
            }
            callbacks->OnSuccess();
          },
          std::move(callbacks))));
}

void WebServiceWorkerRegistrationImpl::GetNavigationPreloadState(
    std::unique_ptr<WebGetNavigationPreloadStateCallbacks> callbacks) {
  DCHECK(state_ == LifecycleState::kAttachedAndBound ||
         state_ == LifecycleState::kUnbound);
  GetRegistrationObjectHost()->GetNavigationPreloadState(
      WrapWebGetNavigationPreloadStateCallbacks(std::move(callbacks)));
}

void WebServiceWorkerRegistrationImpl::SetNavigationPreloadHeader(
    const blink::WebString& value,
    std::unique_ptr<WebSetNavigationPreloadHeaderCallbacks> callbacks) {
  DCHECK(state_ == LifecycleState::kAttachedAndBound ||
         state_ == LifecycleState::kUnbound);
  GetRegistrationObjectHost()->SetNavigationPreloadHeader(
      value.Utf8(),
      WrapResponseCallback(base::BindOnce(
          [](std::unique_ptr<WebSetNavigationPreloadHeaderCallbacks> callbacks,
             blink::mojom::ServiceWorkerErrorType error,
             const base::Optional<std::string>& error_msg) {
            if (error != blink::mojom::ServiceWorkerErrorType::kNone) {
              DCHECK(error_msg);
              callbacks->OnError(blink::WebServiceWorkerError(
                  error, blink::WebString::FromUTF8(*error_msg)));
              return;
            }
            callbacks->OnSuccess();
          },
          std::move(callbacks))));
}

int64_t WebServiceWorkerRegistrationImpl::RegistrationId() const {
  return info_->registration_id;
}

// static
std::unique_ptr<blink::WebServiceWorkerRegistration::Handle>
WebServiceWorkerRegistrationImpl::CreateHandle(
    scoped_refptr<WebServiceWorkerRegistrationImpl> registration) {
  if (!registration)
    return nullptr;
  return std::make_unique<ServiceWorkerRegistrationHandleImpl>(
      std::move(registration));
}

// static
void WebServiceWorkerRegistrationImpl::Destruct(
    const WebServiceWorkerRegistrationImpl* impl) {
  const_cast<WebServiceWorkerRegistrationImpl*>(impl)->DetachAndMaybeDestroy();
}

WebServiceWorkerRegistrationImpl::WebServiceWorkerRegistrationImpl(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
    base::WeakPtr<ServiceWorkerProviderContext> provider_context)
    : registration_id_(info->registration_id),
      proxy_(nullptr),
      binding_(this),
      creation_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      state_(LifecycleState::kInitial),
      provider_context_for_client_(std::move(provider_context)) {
  Attach(std::move(info));

  if (provider_context_for_client_)
    provider_context_for_client_->AddServiceWorkerRegistration(registration_id_,
                                                               this);
}

WebServiceWorkerRegistrationImpl::~WebServiceWorkerRegistrationImpl() {
  DCHECK_EQ(LifecycleState::kDead, state_);
  if (provider_context_for_client_)
    provider_context_for_client_->RemoveServiceWorkerRegistration(
        registration_id_);
}

void WebServiceWorkerRegistrationImpl::SetInstalling(
    blink::mojom::ServiceWorkerObjectInfoPtr info) {
  if (state_ == LifecycleState::kDetached)
    return;
  DCHECK_EQ(LifecycleState::kAttachedAndBound, state_);

  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  scoped_refptr<WebServiceWorkerImpl> service_worker =
      dispatcher->GetOrCreateServiceWorker(std::move(info));
  if (proxy_)
    proxy_->SetInstalling(WebServiceWorkerImpl::CreateHandle(service_worker));
  else
    queued_tasks_.push_back(QueuedTask(INSTALLING, service_worker));
}

void WebServiceWorkerRegistrationImpl::SetWaiting(
    blink::mojom::ServiceWorkerObjectInfoPtr info) {
  if (state_ == LifecycleState::kDetached)
    return;
  DCHECK_EQ(LifecycleState::kAttachedAndBound, state_);

  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  scoped_refptr<WebServiceWorkerImpl> service_worker =
      dispatcher->GetOrCreateServiceWorker(std::move(info));
  if (proxy_)
    proxy_->SetWaiting(WebServiceWorkerImpl::CreateHandle(service_worker));
  else
    queued_tasks_.push_back(QueuedTask(WAITING, service_worker));
}

void WebServiceWorkerRegistrationImpl::SetActive(
    blink::mojom::ServiceWorkerObjectInfoPtr info) {
  if (state_ == LifecycleState::kDetached)
    return;
  DCHECK_EQ(LifecycleState::kAttachedAndBound, state_);

  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  scoped_refptr<WebServiceWorkerImpl> service_worker =
      dispatcher->GetOrCreateServiceWorker(std::move(info));
  if (proxy_)
    proxy_->SetActive(WebServiceWorkerImpl::CreateHandle(service_worker));
  else
    queued_tasks_.push_back(QueuedTask(ACTIVE, service_worker));
}

void WebServiceWorkerRegistrationImpl::RefreshVersionAttributes() {
  SetInstalling(std::move(info_->installing));
  SetWaiting(std::move(info_->waiting));
  SetActive(std::move(info_->active));
}

void WebServiceWorkerRegistrationImpl::SetVersionAttributes(
    int changed_mask,
    blink::mojom::ServiceWorkerObjectInfoPtr installing,
    blink::mojom::ServiceWorkerObjectInfoPtr waiting,
    blink::mojom::ServiceWorkerObjectInfoPtr active) {
  if (!creation_task_runner_->RunsTasksInCurrentSequence()) {
    // As this posted task will definitely run before OnConnectionError() on the
    // |creation_task_runner_|, using base::Unretained() here is safe.
    creation_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebServiceWorkerRegistrationImpl::SetVersionAttributes,
                       base::Unretained(this), changed_mask,
                       std::move(installing), std::move(waiting),
                       std::move(active)));
    return;
  }

  if (state_ == LifecycleState::kDetached)
    return;
  DCHECK_EQ(LifecycleState::kAttachedAndBound, state_);
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  ChangedVersionAttributesMask mask(changed_mask);
  DCHECK(mask.installing_changed() || !installing);
  if (mask.installing_changed()) {
    SetInstalling(std::move(installing));
  }
  DCHECK(mask.waiting_changed() || !waiting);
  if (mask.waiting_changed()) {
    SetWaiting(std::move(waiting));
  }
  DCHECK(mask.active_changed() || !active);
  if (mask.active_changed()) {
    SetActive(std::move(active));
  }
}

void WebServiceWorkerRegistrationImpl::UpdateFound() {
  if (!creation_task_runner_->RunsTasksInCurrentSequence()) {
    // As this posted task will definitely run before OnConnectionError() on the
    // |creation_task_runner_|, using base::Unretained() here is safe.
    creation_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebServiceWorkerRegistrationImpl::UpdateFound,
                       base::Unretained(this)));
    return;
  }

  if (state_ == LifecycleState::kDetached)
    return;
  DCHECK_EQ(LifecycleState::kAttachedAndBound, state_);
  if (proxy_)
    proxy_->DispatchUpdateFoundEvent();
  else
    queued_tasks_.push_back(QueuedTask(UPDATE_FOUND, nullptr));
}

WebServiceWorkerRegistrationImpl::ResponseCallback
WebServiceWorkerRegistrationImpl::WrapResponseCallback(
    ResponseCallback callback) {
  if (!web_callbacks_holder_) {
    web_callbacks_holder_ = std::make_unique<WebCallbacksHolder>();
  }
  return web_callbacks_holder_->WrapResponseCallback(std::move(callback));
}

blink::mojom::ServiceWorkerRegistrationObjectHost::
    GetNavigationPreloadStateCallback
    WebServiceWorkerRegistrationImpl::WrapWebGetNavigationPreloadStateCallbacks(
        std::unique_ptr<WebGetNavigationPreloadStateCallbacks> callbacks) {
  if (!web_callbacks_holder_) {
    web_callbacks_holder_ = std::make_unique<WebCallbacksHolder>();
  }
  return web_callbacks_holder_->WrapWebGetNavigationPreloadStateCallbacks(
      std::move(callbacks));
}

}  // namespace content
