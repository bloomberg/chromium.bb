// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#include "content/renderer/loader/resource_dispatcher.h"

#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_scheduler/post_task.h"
#include "build/build_config.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/navigation_params.h"
#include "content/common/throttling_url_loader.h"
#include "content/public/common/content_features.h"
#include "content/public/common/resource_type.h"
#include "content/public/renderer/fixed_received_data.h"
#include "content/public/renderer/request_peer.h"
#include "content/public/renderer/resource_dispatcher_delegate.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/site_isolation_stats_gatherer.h"
#include "content/renderer/loader/sync_load_context.h"
#include "content/renderer/loader/sync_load_response.h"
#include "content/renderer/loader/url_loader_client_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

namespace content {

namespace {

// Converts |time| from a remote to local TimeTicks, overwriting the original
// value.
void RemoteToLocalTimeTicks(
    const InterProcessTimeTicksConverter& converter,
    base::TimeTicks* time) {
  RemoteTimeTicks remote_time = RemoteTimeTicks::FromTimeTicks(*time);
  *time = converter.ToLocalTimeTicks(remote_time).ToTimeTicks();
}

void CheckSchemeForReferrerPolicy(const network::ResourceRequest& request) {
  if ((request.referrer_policy == Referrer::GetDefaultReferrerPolicy() ||
       request.referrer_policy ==
           net::URLRequest::
               CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE) &&
      request.referrer.SchemeIsCryptographic() &&
      !request.url.SchemeIsCryptographic()) {
    LOG(FATAL) << "Trying to send secure referrer for insecure request "
               << "without an appropriate referrer policy.\n"
               << "URL = " << request.url << "\n"
               << "Referrer = " << request.referrer;
  }
}

void NotifySubresourceStarted(
    scoped_refptr<base::SingleThreadTaskRunner> thread_task_runner,
    int render_frame_id,
    const GURL& url,
    const GURL& referrer,
    const std::string& method,
    ResourceType resource_type,
    const std::string& ip,
    uint32_t cert_status) {
  if (!thread_task_runner)
    return;

  if (!thread_task_runner->BelongsToCurrentThread()) {
    thread_task_runner->PostTask(
        FROM_HERE, base::BindOnce(NotifySubresourceStarted, thread_task_runner,
                                  render_frame_id, url, referrer, method,
                                  resource_type, ip, cert_status));
    return;
  }

  RenderFrameImpl* render_frame =
      RenderFrameImpl::FromRoutingID(render_frame_id);
  if (!render_frame)
    return;

  render_frame->GetFrameHost()->SubresourceResponseStarted(
      url, referrer, method, resource_type, ip, cert_status);
}

}  // namespace

// static
int ResourceDispatcher::MakeRequestID() {
  // NOTE: The resource_dispatcher_host also needs probably unique
  // request_ids, so they count down from -2 (-1 is a special "we're
  // screwed value"), while the renderer process counts up.
  static base::AtomicSequenceNumber sequence;
  return sequence.GetNext();  // We start at zero.
}

ResourceDispatcher::ResourceDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner> thread_task_runner)
    : delegate_(nullptr),
      thread_task_runner_(thread_task_runner),
      weak_factory_(this) {}

ResourceDispatcher::~ResourceDispatcher() {
}

ResourceDispatcher::PendingRequestInfo*
ResourceDispatcher::GetPendingRequestInfo(int request_id) {
  PendingRequestMap::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    // This might happen for kill()ed requests on the webkit end.
    return nullptr;
  }
  return it->second.get();
}

void ResourceDispatcher::OnUploadProgress(int request_id,
                                          int64_t position,
                                          int64_t size) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  request_info->peer->OnUploadProgress(position, size);
}

void ResourceDispatcher::OnReceivedResponse(
    int request_id,
    const network::ResourceResponseHead& response_head) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnReceivedResponse");
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;
  request_info->response_start = base::TimeTicks::Now();

  if (delegate_) {
    std::unique_ptr<RequestPeer> new_peer = delegate_->OnReceivedResponse(
        std::move(request_info->peer), response_head.mime_type,
        request_info->url);
    DCHECK(new_peer);
    request_info->peer = std::move(new_peer);
  }

  if (!IsResourceTypeFrame(request_info->resource_type)) {
    NotifySubresourceStarted(
        RenderThreadImpl::DeprecatedGetMainTaskRunner(),
        request_info->render_frame_id, request_info->response_url,
        request_info->response_referrer, request_info->response_method,
        request_info->resource_type, response_head.socket_address.host(),
        response_head.cert_status);
  }

  network::ResourceResponseInfo renderer_response_info;
  ToResourceResponseInfo(*request_info, response_head, &renderer_response_info);
  request_info->site_isolation_metadata =
      SiteIsolationStatsGatherer::OnReceivedResponse(
          request_info->frame_origin, request_info->response_url,
          request_info->resource_type, renderer_response_info);
  request_info->peer->OnReceivedResponse(renderer_response_info);
}

void ResourceDispatcher::OnReceivedCachedMetadata(
    int request_id,
    const std::vector<uint8_t>& data) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  if (data.size()) {
    request_info->peer->OnReceivedCachedMetadata(
        reinterpret_cast<const char*>(&data.front()), data.size());
  }
}

void ResourceDispatcher::OnDownloadedData(int request_id,
                                          int data_len,
                                          int encoded_data_length) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  request_info->peer->OnDownloadedData(data_len, encoded_data_length);
}

void ResourceDispatcher::OnReceivedRedirect(
    int request_id,
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnReceivedRedirect");
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;
  request_info->response_start = base::TimeTicks::Now();

  network::ResourceResponseInfo renderer_response_info;
  ToResourceResponseInfo(*request_info, response_head, &renderer_response_info);
  if (request_info->peer->OnReceivedRedirect(redirect_info,
                                             renderer_response_info)) {
    // Double-check if the request is still around. The call above could
    // potentially remove it.
    request_info = GetPendingRequestInfo(request_id);
    if (!request_info)
      return;
    // We update the response_url here so that we can send it to
    // SiteIsolationStatsGatherer later when OnReceivedResponse is called.
    request_info->response_url = redirect_info.new_url;
    request_info->response_method = redirect_info.new_method;
    request_info->response_referrer = GURL(redirect_info.new_referrer);
    request_info->has_pending_redirect = true;
    if (!request_info->is_deferred) {
      FollowPendingRedirect(request_info);
    }
  } else {
    Cancel(request_id, std::move(task_runner));
  }
}

void ResourceDispatcher::FollowPendingRedirect(
    PendingRequestInfo* request_info) {
  if (request_info->has_pending_redirect) {
    request_info->has_pending_redirect = false;
    request_info->url_loader->FollowRedirect();
  }
}

void ResourceDispatcher::OnRequestComplete(
    int request_id,
    const network::URLLoaderCompletionStatus& status) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnRequestComplete");

  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;
  request_info->completion_time = base::TimeTicks::Now();
  request_info->buffer.reset();
  request_info->buffer_size = 0;

  RequestPeer* peer = request_info->peer.get();

  if (delegate_) {
    std::unique_ptr<RequestPeer> new_peer = delegate_->OnRequestComplete(
        std::move(request_info->peer), request_info->resource_type,
        status.error_code);
    DCHECK(new_peer);
    request_info->peer = std::move(new_peer);
  }

  // The request ID will be removed from our pending list in the destructor.
  // Normally, dispatching this message causes the reference-counted request to
  // die immediately.
  // TODO(kinuko): Revisit here. This probably needs to call request_info->peer
  // but the past attempt to change it seems to have caused crashes.
  // (crbug.com/547047)
  network::URLLoaderCompletionStatus renderer_status(status);
  renderer_status.completion_time =
      ToRendererCompletionTime(*request_info, status.completion_time);
  peer->OnCompletedRequest(renderer_status);
}

bool ResourceDispatcher::RemovePendingRequest(
    int request_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  PendingRequestMap::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return false;

  // Cancel loading.
  it->second->url_loader = nullptr;
  // Clear URLLoaderClient to stop receiving further Mojo IPC from the browser
  // process.
  it->second->url_loader_client = nullptr;

  // Always delete the pending_request asyncly so that cancelling the request
  // doesn't delete the request context info while its response is still being
  // handled.
  task_runner->DeleteSoon(FROM_HERE, it->second.release());
  pending_requests_.erase(it);

  return true;
}

void ResourceDispatcher::Cancel(
    int request_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  PendingRequestMap::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    DVLOG(1) << "unknown request";
    return;
  }

  // Cancel the request if it didn't complete, and clean it up so the bridge
  // will receive no more messages.
  RemovePendingRequest(request_id, std::move(task_runner));
}

void ResourceDispatcher::SetDefersLoading(int request_id, bool value) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info) {
    DLOG(ERROR) << "unknown request";
    return;
  }
  if (value) {
    request_info->is_deferred = value;
    request_info->url_loader_client->SetDefersLoading();
  } else if (request_info->is_deferred) {
    request_info->is_deferred = false;
    request_info->url_loader_client->UnsetDefersLoading();

    FollowPendingRedirect(request_info);
  }
}

void ResourceDispatcher::DidChangePriority(int request_id,
                                           net::RequestPriority new_priority,
                                           int intra_priority_value) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  DCHECK(request_info);
  request_info->url_loader->SetPriority(new_priority, intra_priority_value);
}

void ResourceDispatcher::OnTransferSizeUpdated(int request_id,
                                               int32_t transfer_size_diff) {
  DCHECK_GT(transfer_size_diff, 0);
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  // TODO(yhirano): Consider using int64_t in
  // RequestPeer::OnTransferSizeUpdated.
  request_info->peer->OnTransferSizeUpdated(transfer_size_diff);
}

ResourceDispatcher::PendingRequestInfo::PendingRequestInfo(
    std::unique_ptr<RequestPeer> peer,
    ResourceType resource_type,
    int render_frame_id,
    const url::Origin& frame_origin,
    const GURL& request_url,
    const std::string& method,
    const GURL& referrer,
    bool download_to_file)
    : peer(std::move(peer)),
      resource_type(resource_type),
      render_frame_id(render_frame_id),
      url(request_url),
      frame_origin(frame_origin),
      response_url(request_url),
      response_method(method),
      response_referrer(referrer),
      download_to_file(download_to_file),
      request_start(base::TimeTicks::Now()) {}

ResourceDispatcher::PendingRequestInfo::~PendingRequestInfo() {
}

void ResourceDispatcher::StartSync(
    std::unique_ptr<network::ResourceRequest> request,
    int routing_id,
    const url::Origin& frame_origin,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    SyncLoadResponse* response,
    scoped_refptr<SharedURLLoaderFactory> url_loader_factory,
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles) {
  CheckSchemeForReferrerPolicy(*request);

  std::unique_ptr<SharedURLLoaderFactoryInfo> factory_info =
      url_loader_factory->Clone();
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Prepare the configured throttles for use on a separate thread.
  for (const auto& throttle : throttles)
    throttle->DetachFromCurrentSequence();

  // A task is posted to a separate thread to execute the request so that
  // this thread may block on a waitable event. It is safe to pass raw
  // pointers to |sync_load_response| and |event| as this stack frame will
  // survive until the request is complete.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::CreateSingleThreadTaskRunnerWithTraits({});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncLoadContext::StartAsyncWithWaitableEvent,
                     std::move(request), routing_id, task_runner, frame_origin,
                     traffic_annotation, std::move(factory_info),
                     std::move(throttles), base::Unretained(response),
                     base::Unretained(&event)));

  event.Wait();
}

int ResourceDispatcher::StartAsync(
    std::unique_ptr<network::ResourceRequest> request,
    int routing_id,
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
    const url::Origin& frame_origin,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    bool is_sync,
    std::unique_ptr<RequestPeer> peer,
    scoped_refptr<SharedURLLoaderFactory> url_loader_factory,
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints) {
  CheckSchemeForReferrerPolicy(*request);

  // Compute a unique request_id for this renderer process.
  int request_id = MakeRequestID();
  pending_requests_[request_id] = std::make_unique<PendingRequestInfo>(
      std::move(peer), static_cast<ResourceType>(request->resource_type),
      request->render_frame_id, frame_origin, request->url, request->method,
      request->referrer, request->download_to_file);

  if (url_loader_client_endpoints) {
    pending_requests_[request_id]->url_loader_client =
        std::make_unique<URLLoaderClientImpl>(request_id, this,
                                              loading_task_runner);

    loading_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&ResourceDispatcher::ContinueForNavigation,
                                  weak_factory_.GetWeakPtr(), request_id,
                                  std::move(url_loader_client_endpoints)));

    return request_id;
  }

  std::unique_ptr<URLLoaderClientImpl> client(
      new URLLoaderClientImpl(request_id, this, loading_task_runner));

  uint32_t options = network::mojom::kURLLoadOptionNone;
  // TODO(jam): use this flag for ResourceDispatcherHost code path once
  // MojoLoading is the only IPC code path.
  if (base::FeatureList::IsEnabled(features::kNetworkService) &&
      request->fetch_request_context_type != REQUEST_CONTEXT_TYPE_FETCH) {
    // MIME sniffing should be disabled for a request initiated by fetch().
    options |= network::mojom::kURLLoadOptionSniffMimeType;
  }
  if (is_sync) {
    options |= network::mojom::kURLLoadOptionSynchronous;
    request->load_flags |= net::LOAD_IGNORE_LIMITS;
  }

  std::unique_ptr<ThrottlingURLLoader> url_loader =
      ThrottlingURLLoader::CreateLoaderAndStart(
          std::move(url_loader_factory), std::move(throttles), routing_id,
          request_id, options, request.get(), client.get(), traffic_annotation,
          std::move(loading_task_runner));
  pending_requests_[request_id]->url_loader = std::move(url_loader);
  pending_requests_[request_id]->url_loader_client = std::move(client);

  return request_id;
}

void ResourceDispatcher::ToResourceResponseInfo(
    const PendingRequestInfo& request_info,
    const network::ResourceResponseHead& browser_info,
    network::ResourceResponseInfo* renderer_info) const {
  *renderer_info = browser_info;
  if (base::TimeTicks::IsConsistentAcrossProcesses() ||
      request_info.request_start.is_null() ||
      request_info.response_start.is_null() ||
      browser_info.request_start.is_null() ||
      browser_info.response_start.is_null() ||
      browser_info.load_timing.request_start.is_null()) {
    return;
  }
  InterProcessTimeTicksConverter converter(
      LocalTimeTicks::FromTimeTicks(request_info.request_start),
      LocalTimeTicks::FromTimeTicks(request_info.response_start),
      RemoteTimeTicks::FromTimeTicks(browser_info.request_start),
      RemoteTimeTicks::FromTimeTicks(browser_info.response_start));

  net::LoadTimingInfo* load_timing = &renderer_info->load_timing;
  RemoteToLocalTimeTicks(converter, &load_timing->request_start);
  RemoteToLocalTimeTicks(converter, &load_timing->proxy_resolve_start);
  RemoteToLocalTimeTicks(converter, &load_timing->proxy_resolve_end);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.dns_start);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.dns_end);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.connect_start);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.connect_end);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.ssl_start);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.ssl_end);
  RemoteToLocalTimeTicks(converter, &load_timing->send_start);
  RemoteToLocalTimeTicks(converter, &load_timing->send_end);
  RemoteToLocalTimeTicks(converter, &load_timing->receive_headers_end);
  RemoteToLocalTimeTicks(converter, &load_timing->push_start);
  RemoteToLocalTimeTicks(converter, &load_timing->push_end);
  RemoteToLocalTimeTicks(converter, &renderer_info->service_worker_start_time);
  RemoteToLocalTimeTicks(converter, &renderer_info->service_worker_ready_time);
}

base::TimeTicks ResourceDispatcher::ToRendererCompletionTime(
    const PendingRequestInfo& request_info,
    const base::TimeTicks& browser_completion_time) const {
  if (request_info.completion_time.is_null()) {
    return browser_completion_time;
  }

  // TODO(simonjam): The optimal lower bound should be the most recent value of
  // TimeTicks::Now() returned to WebKit. Is it worth trying to cache that?
  // Until then, |response_start| is used as it is the most recent value
  // returned for this request.
  int64_t result = std::max(browser_completion_time.ToInternalValue(),
                            request_info.response_start.ToInternalValue());
  result = std::min(result, request_info.completion_time.ToInternalValue());
  return base::TimeTicks::FromInternalValue(result);
}

void ResourceDispatcher::ContinueForNavigation(
    int request_id,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints) {
  DCHECK(url_loader_client_endpoints);
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  URLLoaderClientImpl* client_ptr = request_info->url_loader_client.get();

  // Short circuiting call to OnReceivedResponse to immediately start
  // the request. ResourceResponseHead can be empty here because we
  // pull the StreamOverride's one in
  // WebURLLoaderImpl::Context::OnReceivedResponse.
  client_ptr->OnReceiveResponse(network::ResourceResponseHead(), base::nullopt,
                                network::mojom::DownloadedTempFilePtr());
  // TODO(clamy): Move the replaying of redirects from WebURLLoaderImpl here.

  // Abort if the request is cancelled.
  if (!GetPendingRequestInfo(request_id))
    return;

  client_ptr->Bind(std::move(url_loader_client_endpoints));
}

}  // namespace content
