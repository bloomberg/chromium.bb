// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/web_service_worker_registration_impl.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/child_process.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_provider_impl.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebNavigationPreloadState.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistrationProxy.h"

namespace content {

namespace {

class HandleImpl : public blink::WebServiceWorkerRegistration::Handle {
 public:
  explicit HandleImpl(
      const scoped_refptr<WebServiceWorkerRegistrationImpl>& registration)
      : registration_(registration) {}
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
  auto* impl = new WebServiceWorkerRegistrationImpl(std::move(info));
  impl->host_for_global_scope_ =
      blink::mojom::ThreadSafeServiceWorkerRegistrationObjectHostAssociatedPtr::
          Create(std::move(impl->info_->host_ptr_info), io_task_runner);
  return impl;
}

// static
scoped_refptr<WebServiceWorkerRegistrationImpl>
WebServiceWorkerRegistrationImpl::CreateForServiceWorkerClient(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info) {
  auto* impl = new WebServiceWorkerRegistrationImpl(std::move(info));
  impl->host_for_client_.Bind(std::move(impl->info_->host_ptr_info));
  return impl;
}

void WebServiceWorkerRegistrationImpl::SetInstalling(
    const scoped_refptr<WebServiceWorkerImpl>& service_worker) {
  if (proxy_)
    proxy_->SetInstalling(WebServiceWorkerImpl::CreateHandle(service_worker));
  else
    queued_tasks_.push_back(QueuedTask(INSTALLING, service_worker));
}

void WebServiceWorkerRegistrationImpl::SetWaiting(
    const scoped_refptr<WebServiceWorkerImpl>& service_worker) {
  if (proxy_)
    proxy_->SetWaiting(WebServiceWorkerImpl::CreateHandle(service_worker));
  else
    queued_tasks_.push_back(QueuedTask(WAITING, service_worker));
}

void WebServiceWorkerRegistrationImpl::SetActive(
    const scoped_refptr<WebServiceWorkerImpl>& service_worker) {
  if (proxy_)
    proxy_->SetActive(WebServiceWorkerImpl::CreateHandle(service_worker));
  else
    queued_tasks_.push_back(QueuedTask(ACTIVE, service_worker));
}

void WebServiceWorkerRegistrationImpl::OnUpdateFound() {
  if (proxy_)
    proxy_->DispatchUpdateFoundEvent();
  else
    queued_tasks_.push_back(QueuedTask(UPDATE_FOUND, nullptr));
}

void WebServiceWorkerRegistrationImpl::SetProxy(
    blink::WebServiceWorkerRegistrationProxy* proxy) {
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
  GetRegistrationObjectHost()->Update(
      base::BindOnce(&WebServiceWorkerRegistrationImpl::OnUpdated,
                     base::Unretained(this), std::move(callbacks)));
}

void WebServiceWorkerRegistrationImpl::Unregister(
    blink::WebServiceWorkerProvider* provider,
    std::unique_ptr<WebServiceWorkerUnregistrationCallbacks> callbacks) {
  WebServiceWorkerProviderImpl* provider_impl =
      static_cast<WebServiceWorkerProviderImpl*>(provider);
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->UnregisterServiceWorker(provider_impl->provider_id(),
                                      RegistrationId(), std::move(callbacks));
}

void WebServiceWorkerRegistrationImpl::EnableNavigationPreload(
    bool enable,
    blink::WebServiceWorkerProvider* provider,
    std::unique_ptr<WebEnableNavigationPreloadCallbacks> callbacks) {
  WebServiceWorkerProviderImpl* provider_impl =
      static_cast<WebServiceWorkerProviderImpl*>(provider);
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->EnableNavigationPreload(provider_impl->provider_id(),
                                      RegistrationId(), enable,
                                      std::move(callbacks));
}

void WebServiceWorkerRegistrationImpl::GetNavigationPreloadState(
    blink::WebServiceWorkerProvider* provider,
    std::unique_ptr<WebGetNavigationPreloadStateCallbacks> callbacks) {
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

// static
std::unique_ptr<blink::WebServiceWorkerRegistration::Handle>
WebServiceWorkerRegistrationImpl::CreateHandle(
    const scoped_refptr<WebServiceWorkerRegistrationImpl>& registration) {
  if (!registration)
    return nullptr;
  return std::make_unique<HandleImpl>(registration);
}

WebServiceWorkerRegistrationImpl::WebServiceWorkerRegistrationImpl(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info)
    : info_(std::move(info)), proxy_(nullptr) {
  DCHECK(info_);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationHandleId,
            info_->handle_id);

  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->AddServiceWorkerRegistration(info_->handle_id, this);
}

WebServiceWorkerRegistrationImpl::~WebServiceWorkerRegistrationImpl() {
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  if (dispatcher)
    dispatcher->RemoveServiceWorkerRegistration(info_->handle_id);
}

}  // namespace content
