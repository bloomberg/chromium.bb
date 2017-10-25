// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/web_service_worker_registration_impl.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/child_process.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/renderer/service_worker/service_worker_dispatcher.h"
#include "content/renderer/service_worker/web_service_worker_impl.h"
#include "content/renderer/service_worker/web_service_worker_provider_impl.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebNavigationPreloadState.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistrationProxy.h"

namespace content {

namespace {

class HandleImpl : public blink::WebServiceWorkerRegistration::Handle {
 public:
  explicit HandleImpl(
      scoped_refptr<WebServiceWorkerRegistrationImpl> registration)
      : registration_(std::move(registration)) {}
  ~HandleImpl() override {}

  blink::WebServiceWorkerRegistration* Registration() override {
    return registration_.get();
  }

 private:
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration_;

  DISALLOW_COPY_AND_ASSIGN(HandleImpl);
};

}  // namespace

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
  auto* impl = new WebServiceWorkerRegistrationImpl(std::move(info));
  impl->host_for_global_scope_ =
      blink::mojom::ThreadSafeServiceWorkerRegistrationObjectHostAssociatedPtr::
          Create(std::move(impl->info_->host_ptr_info), io_task_runner);
  // |impl|'s destruction needs both DetachAndMaybeDestroy() and
  // OnConnectionError() to be called (see comments at LifecycleState enum), and
  // OnConnectionError() cannot happen before BindRequest(), therefore using
  // base::Unretained() here is safe.
  io_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&WebServiceWorkerRegistrationImpl::BindRequest,
                     base::Unretained(impl), std::move(impl->info_->request)));
  impl->state_ = LifecycleState::kAttachedAndBound;
  return impl;
}

// static
scoped_refptr<WebServiceWorkerRegistrationImpl>
WebServiceWorkerRegistrationImpl::CreateForServiceWorkerClient(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info) {
  DCHECK(info->request.is_pending());
  auto* impl = new WebServiceWorkerRegistrationImpl(std::move(info));
  impl->host_for_client_.Bind(std::move(impl->info_->host_ptr_info));
  impl->BindRequest(std::move(impl->info_->request));
  impl->state_ = LifecycleState::kAttachedAndBound;
  return impl;
}

void WebServiceWorkerRegistrationImpl::AttachForServiceWorkerGlobalScope(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  if (state_ == LifecycleState::kAttachedAndBound)
    return;
  DCHECK_EQ(LifecycleState::kDetached, state_);
  DCHECK(!info->request.is_pending());
  Attach(std::move(info));

  DCHECK(!host_for_global_scope_);
  DCHECK(!host_for_client_);
  host_for_global_scope_ =
      blink::mojom::ThreadSafeServiceWorkerRegistrationObjectHostAssociatedPtr::
          Create(std::move(info_->host_ptr_info), io_task_runner);
  state_ = LifecycleState::kAttachedAndBound;
}

void WebServiceWorkerRegistrationImpl::AttachForServiceWorkerClient(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info) {
  if (state_ == LifecycleState::kAttachedAndBound)
    return;
  DCHECK_EQ(LifecycleState::kDetached, state_);
  DCHECK(!info->request.is_pending());
  Attach(std::move(info));

  DCHECK(!host_for_global_scope_);
  DCHECK(!host_for_client_);
  host_for_client_.Bind(std::move(info_->host_ptr_info));
  state_ = LifecycleState::kAttachedAndBound;
}

void WebServiceWorkerRegistrationImpl::SetInstalling(
    const scoped_refptr<WebServiceWorkerImpl>& service_worker) {
  if (state_ == LifecycleState::kDetached)
    return;
  DCHECK_EQ(LifecycleState::kAttachedAndBound, state_);
  if (proxy_)
    proxy_->SetInstalling(WebServiceWorkerImpl::CreateHandle(service_worker));
  else
    queued_tasks_.push_back(QueuedTask(INSTALLING, service_worker));
}

void WebServiceWorkerRegistrationImpl::SetWaiting(
    const scoped_refptr<WebServiceWorkerImpl>& service_worker) {
  if (state_ == LifecycleState::kDetached)
    return;
  DCHECK_EQ(LifecycleState::kAttachedAndBound, state_);
  if (proxy_)
    proxy_->SetWaiting(WebServiceWorkerImpl::CreateHandle(service_worker));
  else
    queued_tasks_.push_back(QueuedTask(WAITING, service_worker));
}

void WebServiceWorkerRegistrationImpl::SetActive(
    const scoped_refptr<WebServiceWorkerImpl>& service_worker) {
  if (state_ == LifecycleState::kDetached)
    return;
  DCHECK_EQ(LifecycleState::kAttachedAndBound, state_);
  if (proxy_)
    proxy_->SetActive(WebServiceWorkerImpl::CreateHandle(service_worker));
  else
    queued_tasks_.push_back(QueuedTask(ACTIVE, service_worker));
}

void WebServiceWorkerRegistrationImpl::OnUpdateFound() {
  if (state_ == LifecycleState::kDetached)
    return;
  DCHECK_EQ(LifecycleState::kAttachedAndBound, state_);
  if (proxy_)
    proxy_->DispatchUpdateFoundEvent();
  else
    queued_tasks_.push_back(QueuedTask(UPDATE_FOUND, nullptr));
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
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationHandleId,
            info->handle_id);
  DCHECK_EQ(handle_id_, info->handle_id);
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
  if (state_ == LifecycleState::kUnbound) {
    state_ = LifecycleState::kDead;
    delete this;
    return;
  }
  DCHECK_EQ(LifecycleState::kAttachedAndBound, state_);
  state_ = LifecycleState::kDetached;
  // We will continue in OnConnectionError() triggered by destruction of the
  // content::ServiceWorkerRegistrationHandle in the browser process, or else in
  // Attach*() if |this| is reused.
}

void WebServiceWorkerRegistrationImpl::BindRequest(
    blink::mojom::ServiceWorkerRegistrationObjectAssociatedRequest request) {
  DCHECK(request.is_pending());
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(
      base::Bind(&WebServiceWorkerRegistrationImpl::OnConnectionError,
                 base::Unretained(this)));
}

void WebServiceWorkerRegistrationImpl::OnConnectionError() {
  if (!creation_task_runner_->RunsTasksInCurrentSequence()) {
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

void WebServiceWorkerRegistrationImpl::Update(
    blink::WebServiceWorkerProvider* provider,
    std::unique_ptr<WebServiceWorkerUpdateCallbacks> callbacks) {
  DCHECK(state_ == LifecycleState::kAttachedAndBound ||
         state_ == LifecycleState::kUnbound);
  GetRegistrationObjectHost()->Update(
      base::BindOnce(&WebServiceWorkerRegistrationImpl::OnUpdated,
                     base::Unretained(this), std::move(callbacks)));
}

void WebServiceWorkerRegistrationImpl::Unregister(
    blink::WebServiceWorkerProvider* provider,
    std::unique_ptr<WebServiceWorkerUnregistrationCallbacks> callbacks) {
  DCHECK(state_ == LifecycleState::kAttachedAndBound ||
         state_ == LifecycleState::kUnbound);
  GetRegistrationObjectHost()->Unregister(
      base::BindOnce(&WebServiceWorkerRegistrationImpl::OnUnregistered,
                     base::Unretained(this), std::move(callbacks)));
}

void WebServiceWorkerRegistrationImpl::EnableNavigationPreload(
    bool enable,
    blink::WebServiceWorkerProvider* provider,
    std::unique_ptr<WebEnableNavigationPreloadCallbacks> callbacks) {
  GetRegistrationObjectHost()->EnableNavigationPreload(
      enable,
      base::BindOnce(
          &WebServiceWorkerRegistrationImpl::OnDidEnableNavigationPreload,
          base::Unretained(this), std::move(callbacks)));
}

void WebServiceWorkerRegistrationImpl::GetNavigationPreloadState(
    blink::WebServiceWorkerProvider* provider,
    std::unique_ptr<WebGetNavigationPreloadStateCallbacks> callbacks) {
  DCHECK(GetRegistrationObjectHost());
  WebServiceWorkerProviderImpl* provider_impl =
      static_cast<WebServiceWorkerProviderImpl*>(provider);
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->GetNavigationPreloadState(provider_impl->provider_id(),
                                        RegistrationId(), std::move(callbacks));
}

void WebServiceWorkerRegistrationImpl::SetNavigationPreloadHeader(
    const blink::WebString& value,
    blink::WebServiceWorkerProvider* provider,
    std::unique_ptr<WebSetNavigationPreloadHeaderCallbacks> callbacks) {
  DCHECK(GetRegistrationObjectHost());
  WebServiceWorkerProviderImpl* provider_impl =
      static_cast<WebServiceWorkerProviderImpl*>(provider);
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->SetNavigationPreloadHeader(provider_impl->provider_id(),
                                         RegistrationId(), value.Utf8(),
                                         std::move(callbacks));
}

int64_t WebServiceWorkerRegistrationImpl::RegistrationId() const {
  return info_->registration_id;
}

void WebServiceWorkerRegistrationImpl::OnUpdated(
    std::unique_ptr<WebServiceWorkerUpdateCallbacks> callbacks,
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
}

void WebServiceWorkerRegistrationImpl::OnUnregistered(
    std::unique_ptr<WebServiceWorkerUnregistrationCallbacks> callbacks,
    blink::mojom::ServiceWorkerErrorType error,
    const base::Optional<std::string>& error_msg) {
  if (error != blink::mojom::ServiceWorkerErrorType::kNone &&
      error != blink::mojom::ServiceWorkerErrorType::kNotFound) {
    DCHECK(error_msg);
    callbacks->OnError(blink::WebServiceWorkerError(
        error, blink::WebString::FromUTF8(*error_msg)));
    return;
  }

  callbacks->OnSuccess(error == blink::mojom::ServiceWorkerErrorType::kNone);
}

void WebServiceWorkerRegistrationImpl::OnDidEnableNavigationPreload(
    std::unique_ptr<WebEnableNavigationPreloadCallbacks> callbacks,
    blink::mojom::ServiceWorkerErrorType error,
    const base::Optional<std::string>& error_msg) {
  if (error != blink::mojom::ServiceWorkerErrorType::kNone) {
    DCHECK(error_msg);
    callbacks->OnError(blink::WebServiceWorkerError(
        error, blink::WebString::FromUTF8(*error_msg)));
    return;
  }
  callbacks->OnSuccess();
}

// static
std::unique_ptr<blink::WebServiceWorkerRegistration::Handle>
WebServiceWorkerRegistrationImpl::CreateHandle(
    scoped_refptr<WebServiceWorkerRegistrationImpl> registration) {
  if (!registration)
    return nullptr;
  return std::make_unique<HandleImpl>(std::move(registration));
}

// static
void WebServiceWorkerRegistrationImpl::Destruct(
    const WebServiceWorkerRegistrationImpl* impl) {
  const_cast<WebServiceWorkerRegistrationImpl*>(impl)->DetachAndMaybeDestroy();
}

WebServiceWorkerRegistrationImpl::WebServiceWorkerRegistrationImpl(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info)
    : handle_id_(info->handle_id),
      proxy_(nullptr),
      binding_(this),
      creation_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      state_(LifecycleState::kInitial) {
  Attach(std::move(info));

  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->AddServiceWorkerRegistration(handle_id_, this);
}

WebServiceWorkerRegistrationImpl::~WebServiceWorkerRegistrationImpl() {
  DCHECK_EQ(LifecycleState::kDead, state_);
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  if (dispatcher)
    dispatcher->RemoveServiceWorkerRegistration(handle_id_);
}

}  // namespace content
