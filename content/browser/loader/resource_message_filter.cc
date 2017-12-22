// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_message_filter.h"

#include "base/logging.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_requester_info.h"
#include "content/browser/loader/url_loader_factory_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/resource_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "storage/browser/fileapi/file_system_context.h"

namespace content {
namespace {
mojom::URLLoaderFactory* g_test_factory;
ResourceMessageFilter* g_current_filter;
}  // namespace

ResourceMessageFilter::ResourceMessageFilter(
    int child_id,
    ChromeAppCacheService* appcache_service,
    ChromeBlobStorageContext* blob_storage_context,
    storage::FileSystemContext* file_system_context,
    ServiceWorkerContextWrapper* service_worker_context,
    const GetContextsCallback& get_contexts_callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_runner)
    : BrowserMessageFilter(ResourceMsgStart),
      BrowserAssociatedInterface<mojom::URLLoaderFactory>(this, this),
      is_channel_closed_(false),
      requester_info_(
          ResourceRequesterInfo::CreateForRenderer(child_id,
                                                   appcache_service,
                                                   blob_storage_context,
                                                   file_system_context,
                                                   service_worker_context,
                                                   get_contexts_callback)),
      io_thread_task_runner_(io_thread_runner),
      weak_ptr_factory_(this) {}

ResourceMessageFilter::~ResourceMessageFilter() {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(is_channel_closed_);
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
}

void ResourceMessageFilter::OnFilterAdded(IPC::Channel*) {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
  InitializeOnIOThread();
}

void ResourceMessageFilter::OnChannelClosing() {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());

  // Close all additional Mojo connections opened to this object so that
  // messages are not dispatched while it is being shut down.
  bindings_.CloseAllBindings();

  // Unhook us from all pending network requests so they don't get sent to a
  // deleted object.
  ResourceDispatcherHostImpl::Get()->CancelRequestsForProcess(
      requester_info_->child_id());

  weak_ptr_factory_.InvalidateWeakPtrs();
  is_channel_closed_ = true;
}

bool ResourceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
  // Check if InitializeOnIOThread() has been called.
  DCHECK_EQ(this, requester_info_->filter());
  return ResourceDispatcherHostImpl::Get()->OnMessageReceived(
      message, requester_info_.get());
}

void ResourceMessageFilter::OnDestruct() const {
  // Destroy the filter on the IO thread since that's where its weak pointers
  // are being used.
  if (io_thread_task_runner_->BelongsToCurrentThread()) {
    delete this;
  } else {
    io_thread_task_runner_->DeleteSoon(FROM_HERE, this);
  }
}

base::WeakPtr<ResourceMessageFilter> ResourceMessageFilter::GetWeakPtr() {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
  return is_channel_closed_ ? nullptr : weak_ptr_factory_.GetWeakPtr();
}

void ResourceMessageFilter::CreateLoaderAndStart(
    mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& url_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (g_test_factory && !g_current_filter) {
    g_current_filter = this;
    g_test_factory->CreateLoaderAndStart(std::move(request), routing_id,
                                         request_id, options, url_request,
                                         std::move(client), traffic_annotation);
    g_current_filter = nullptr;
    return;
  }

  URLLoaderFactoryImpl::CreateLoaderAndStart(
      requester_info_.get(), std::move(request), routing_id, request_id,
      options, url_request, std::move(client),
      net::NetworkTrafficAnnotationTag(traffic_annotation));
}

void ResourceMessageFilter::Clone(mojom::URLLoaderFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

int ResourceMessageFilter::child_id() const {
  return requester_info_->child_id();
}

void ResourceMessageFilter::InitializeForTest() {
  InitializeOnIOThread();
}

void ResourceMessageFilter::SetNetworkFactoryForTesting(
    mojom::URLLoaderFactory* test_factory) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::IO) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!test_factory || !g_test_factory);
  g_test_factory = test_factory;
}

ResourceMessageFilter* ResourceMessageFilter::GetCurrentForTesting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return g_current_filter;
}

void ResourceMessageFilter::InitializeOnIOThread() {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
  // The WeakPtr of the filter must be created on the IO thread. So sets the
  // WeakPtr of |requester_info_| now.
  requester_info_->set_filter(GetWeakPtr());
}

}  // namespace content
