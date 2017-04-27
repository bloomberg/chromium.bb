// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#include "content/child/resource_dispatcher.h"

#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/child/request_extra_data.h"
#include "content/child/resource_scheduling_filter.h"
#include "content/child/shared_memory_received_data_factory.h"
#include "content/child/site_isolation_stats_gatherer.h"
#include "content/child/sync_load_response.h"
#include "content/child/url_loader_client_impl.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/navigation_params.h"
#include "content/common/resource_messages.h"
#include "content/common/resource_request.h"
#include "content/common/resource_request_completion_status.h"
#include "content/public/child/fixed_received_data.h"
#include "content/public/child/request_peer.h"
#include "content/public/child/resource_dispatcher_delegate.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/resource_type.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"

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

void CrashOnMapFailure() {
#if defined(OS_WIN)
  DWORD last_err = GetLastError();
  base::debug::Alias(&last_err);
#endif
  CHECK(false);
}

// Each resource request is assigned an ID scoped to this process.
int MakeRequestID() {
  // NOTE: The resource_dispatcher_host also needs probably unique
  // request_ids, so they count down from -2 (-1 is a special we're
  // screwed value), while the renderer process counts up.
  static base::StaticAtomicSequenceNumber sequence;
  return sequence.GetNext();  // We start at zero.
}

void CheckSchemeForReferrerPolicy(const ResourceRequest& request) {
  if ((request.referrer_policy == blink::kWebReferrerPolicyDefault ||
       request.referrer_policy ==
           blink::kWebReferrerPolicyNoReferrerWhenDowngrade) &&
      request.referrer.SchemeIsCryptographic() &&
      !request.url.SchemeIsCryptographic()) {
    LOG(FATAL) << "Trying to send secure referrer for insecure request "
               << "without an appropriate referrer policy.\n"
               << "URL = " << request.url << "\n"
               << "Referrer = " << request.referrer;
  }
}

}  // namespace

ResourceDispatcher::ResourceDispatcher(
    IPC::Sender* sender,
    scoped_refptr<base::SingleThreadTaskRunner> thread_task_runner)
    : message_sender_(sender),
      delegate_(NULL),
      io_timestamp_(base::TimeTicks()),
      thread_task_runner_(thread_task_runner),
      weak_factory_(this) {}

ResourceDispatcher::~ResourceDispatcher() {
}

bool ResourceDispatcher::OnMessageReceived(const IPC::Message& message) {
  if (!IsResourceDispatcherMessage(message)) {
    return false;
  }

  int request_id;

  base::PickleIterator iter(message);
  if (!iter.ReadInt(&request_id)) {
    NOTREACHED() << "malformed resource message";
    return true;
  }

  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info) {
    // Release resources in the message if it is a data message.
    ReleaseResourcesInDataMessage(message);
    return true;
  }

  if (request_info->is_deferred) {
    request_info->deferred_message_queue.push_back(new IPC::Message(message));
    return true;
  }

  // Make sure any deferred messages are dispatched before we dispatch more.
  if (!request_info->deferred_message_queue.empty()) {
    request_info->deferred_message_queue.push_back(new IPC::Message(message));
    FlushDeferredMessages(request_id);
    return true;
  }

  DispatchMessage(message);
  return true;
}

ResourceDispatcher::PendingRequestInfo*
ResourceDispatcher::GetPendingRequestInfo(int request_id) {
  PendingRequestMap::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    // This might happen for kill()ed requests on the webkit end.
    return NULL;
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

  // URLLoaderClientImpl has its own acknowledgement, and doesn't need the IPC
  // message here.
  if (!request_info->url_loader) {
    // Acknowledge receipt
    message_sender_->Send(new ResourceHostMsg_UploadProgress_ACK(request_id));
  }
}

void ResourceDispatcher::OnReceivedResponse(
    int request_id, const ResourceResponseHead& response_head) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnReceivedResponse");
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;
  request_info->response_start = ConsumeIOTimestamp();

  if (delegate_) {
    std::unique_ptr<RequestPeer> new_peer = delegate_->OnReceivedResponse(
        std::move(request_info->peer), response_head.mime_type,
        request_info->url);
    DCHECK(new_peer);
    request_info->peer = std::move(new_peer);
  }

  ResourceResponseInfo renderer_response_info;
  ToResourceResponseInfo(*request_info, response_head, &renderer_response_info);
  request_info->site_isolation_metadata =
      SiteIsolationStatsGatherer::OnReceivedResponse(
          request_info->frame_origin, request_info->response_url,
          request_info->resource_type, request_info->origin_pid,
          renderer_response_info);
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

void ResourceDispatcher::OnSetDataBuffer(int request_id,
                                         base::SharedMemoryHandle shm_handle,
                                         int shm_size,
                                         base::ProcessId renderer_pid) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnSetDataBuffer");
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  bool shm_valid = base::SharedMemory::IsHandleValid(shm_handle);
  CHECK((shm_valid && shm_size > 0) || (!shm_valid && !shm_size));

  request_info->buffer.reset(
      new base::SharedMemory(shm_handle, true));  // read only
  request_info->received_data_factory =
      make_scoped_refptr(new SharedMemoryReceivedDataFactory(
          message_sender_, request_id, request_info->buffer));

  bool ok = request_info->buffer->Map(shm_size);
  if (!ok) {
    // Added to help debug crbug/160401.
    base::ProcessId renderer_pid_copy = renderer_pid;
    base::debug::Alias(&renderer_pid_copy);

    base::SharedMemoryHandle shm_handle_copy = shm_handle;
    base::debug::Alias(&shm_handle_copy);

    CrashOnMapFailure();
    return;
  }

  // TODO(erikchen): Temporary debugging. http://crbug.com/527588.
  CHECK_GE(shm_size, 0);
  CHECK_LE(shm_size, 512 * 1024);
  request_info->buffer_size = shm_size;
}

void ResourceDispatcher::OnReceivedData(int request_id,
                                        int data_offset,
                                        int data_length,
                                        int encoded_data_length) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnReceivedData");
  DCHECK_GT(data_length, 0);
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  bool send_ack = true;
  if (request_info && data_length > 0) {
    CHECK(base::SharedMemory::IsHandleValid(request_info->buffer->handle()));
    CHECK_GE(request_info->buffer_size, data_offset + data_length);

    const char* data_start = static_cast<char*>(request_info->buffer->memory());
    CHECK(data_start);
    CHECK(data_start + data_offset);
    const char* data_ptr = data_start + data_offset;

    // Check whether this response data is compliant with our cross-site
    // document blocking policy. We only do this for the first chunk of data.
    if (request_info->site_isolation_metadata.get()) {
      SiteIsolationStatsGatherer::OnReceivedFirstChunk(
          request_info->site_isolation_metadata, data_ptr, data_length);
      request_info->site_isolation_metadata.reset();
    }

    std::unique_ptr<RequestPeer::ReceivedData> data =
        request_info->received_data_factory->Create(data_offset, data_length);
    // |data| takes care of ACKing.
    send_ack = false;
    request_info->peer->OnReceivedData(std::move(data));
  }

  // Get the request info again as the client callback may modify the info.
  request_info = GetPendingRequestInfo(request_id);
  if (request_info && encoded_data_length > 0)
    request_info->peer->OnTransferSizeUpdated(encoded_data_length);

  // Acknowledge the reception of this data.
  if (send_ack)
    message_sender_->Send(new ResourceHostMsg_DataReceived_ACK(request_id));
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
    const ResourceResponseHead& response_head) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnReceivedRedirect");
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;
  request_info->response_start = ConsumeIOTimestamp();

  ResourceResponseInfo renderer_response_info;
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
    request_info->pending_redirect_message.reset(
        new ResourceHostMsg_FollowRedirect(request_id));
    if (!request_info->is_deferred) {
      FollowPendingRedirect(request_id, request_info);
    }
  } else {
    Cancel(request_id);
  }
}

void ResourceDispatcher::FollowPendingRedirect(
    int request_id,
    PendingRequestInfo* request_info) {
  IPC::Message* msg = request_info->pending_redirect_message.release();
  if (msg) {
    if (request_info->url_loader) {
      request_info->url_loader->FollowRedirect();
      delete msg;
    } else {
      message_sender_->Send(msg);
    }
  }
}

void ResourceDispatcher::OnRequestComplete(
    int request_id,
    const ResourceRequestCompletionStatus& request_complete_data) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnRequestComplete");

  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;
  request_info->completion_time = ConsumeIOTimestamp();
  request_info->buffer.reset();
  if (request_info->received_data_factory)
    request_info->received_data_factory->Stop();
  request_info->received_data_factory = nullptr;
  request_info->buffer_size = 0;

  RequestPeer* peer = request_info->peer.get();

  if (delegate_) {
    std::unique_ptr<RequestPeer> new_peer = delegate_->OnRequestComplete(
        std::move(request_info->peer), request_info->resource_type,
        request_complete_data.error_code);
    DCHECK(new_peer);
    request_info->peer = std::move(new_peer);
  }

  base::TimeTicks renderer_completion_time = ToRendererCompletionTime(
      *request_info, request_complete_data.completion_time);

  // The request ID will be removed from our pending list in the destructor.
  // Normally, dispatching this message causes the reference-counted request to
  // die immediately.
  // TODO(kinuko): Revisit here. This probably needs to call request_info->peer
  // but the past attempt to change it seems to have caused crashes.
  // (crbug.com/547047)
  peer->OnCompletedRequest(request_complete_data.error_code,
                           request_complete_data.was_ignored_by_handler,
                           request_complete_data.exists_in_cache,
                           renderer_completion_time,
                           request_complete_data.encoded_data_length,
                           request_complete_data.encoded_body_length,
                           request_complete_data.decoded_body_length);
}

bool ResourceDispatcher::RemovePendingRequest(int request_id) {
  PendingRequestMap::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return false;

  PendingRequestInfo* request_info = it->second.get();

  // |url_loader_client| releases the downloaded file. Otherwise (i.e., we
  // are using Chrome IPC), we should release it here.
  bool release_downloaded_file =
      request_info->download_to_file && !it->second->url_loader_client;

  ReleaseResourcesInMessageQueue(&request_info->deferred_message_queue);

  // Cancel loading.
  it->second->url_loader = nullptr;
  // Clear URLLoaderClient to stop receiving further Mojo IPC from the browser
  // process.
  it->second->url_loader_client = nullptr;

  // Always delete the pending_request asyncly so that cancelling the request
  // doesn't delete the request context info while its response is still being
  // handled.
  thread_task_runner_->DeleteSoon(FROM_HERE, it->second.release());
  pending_requests_.erase(it);

  if (release_downloaded_file) {
    message_sender_->Send(
        new ResourceHostMsg_ReleaseDownloadedFile(request_id));
  }

  if (resource_scheduling_filter_.get())
    resource_scheduling_filter_->ClearRequestIdTaskRunner(request_id);

  return true;
}

void ResourceDispatcher::Cancel(int request_id) {
  PendingRequestMap::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    DVLOG(1) << "unknown request";
    return;
  }

  // |completion_time.is_null()| is a proxy for OnRequestComplete never being
  // called.
  // TODO(csharrison): Remove this code when crbug.com/557430 is resolved.
  // Sample this enough that this won't dump much more than a hundred times a
  // day even without the static guard. The guard ensures this dumps much less
  // frequently, because these aborts frequently come in quick succession.
  const PendingRequestInfo& info = *it->second;
  int64_t request_time =
      (base::TimeTicks::Now() - info.request_start).InMilliseconds();
  if (info.resource_type == ResourceType::RESOURCE_TYPE_MAIN_FRAME &&
      info.completion_time.is_null() && request_time < 100 &&
      base::RandDouble() < .000001) {
    static bool should_dump = true;
    if (should_dump) {
      char url_copy[256] = {0};
      strncpy(url_copy, info.response_url.spec().c_str(),
              sizeof(url_copy));
      base::debug::Alias(&url_copy);
      base::debug::Alias(&request_time);
      base::debug::DumpWithoutCrashing();
      should_dump = false;
    }
  }
  // Cancel the request if it didn't complete, and clean it up so the bridge
  // will receive no more messages.
  if (info.completion_time.is_null() && !info.url_loader)
    message_sender_->Send(new ResourceHostMsg_CancelRequest(request_id));
  RemovePendingRequest(request_id);
}

void ResourceDispatcher::SetDefersLoading(int request_id, bool value) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info) {
    DLOG(ERROR) << "unknown request";
    return;
  }
  if (value) {
    request_info->is_deferred = value;
    if (request_info->url_loader_client)
      request_info->url_loader_client->SetDefersLoading();
  } else if (request_info->is_deferred) {
    request_info->is_deferred = false;

    if (request_info->url_loader_client)
      request_info->url_loader_client->UnsetDefersLoading();

    FollowPendingRedirect(request_id, request_info);

    thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ResourceDispatcher::FlushDeferredMessages,
                              weak_factory_.GetWeakPtr(), request_id));
  }
}

void ResourceDispatcher::DidChangePriority(int request_id,
                                           net::RequestPriority new_priority,
                                           int intra_priority_value) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  DCHECK(request_info);
  if (request_info->url_loader) {
    request_info->url_loader->SetPriority(new_priority, intra_priority_value);
  } else {
    message_sender_->Send(new ResourceHostMsg_DidChangePriority(
        request_id, new_priority, intra_priority_value));
  }
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
    int origin_pid,
    const url::Origin& frame_origin,
    const GURL& request_url,
    bool download_to_file)
    : peer(std::move(peer)),
      resource_type(resource_type),
      origin_pid(origin_pid),
      url(request_url),
      frame_origin(frame_origin),
      response_url(request_url),
      download_to_file(download_to_file),
      request_start(base::TimeTicks::Now()) {}

ResourceDispatcher::PendingRequestInfo::~PendingRequestInfo() {
}

void ResourceDispatcher::DispatchMessage(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(ResourceDispatcher, message)
    IPC_MESSAGE_HANDLER(ResourceMsg_UploadProgress, OnUploadProgress)
    IPC_MESSAGE_HANDLER(ResourceMsg_ReceivedResponse, OnReceivedResponse)
    IPC_MESSAGE_HANDLER(ResourceMsg_ReceivedCachedMetadata,
                        OnReceivedCachedMetadata)
    IPC_MESSAGE_HANDLER(ResourceMsg_ReceivedRedirect, OnReceivedRedirect)
    IPC_MESSAGE_HANDLER(ResourceMsg_SetDataBuffer, OnSetDataBuffer)
    IPC_MESSAGE_HANDLER(ResourceMsg_DataReceived, OnReceivedData)
    IPC_MESSAGE_HANDLER(ResourceMsg_DataDownloaded, OnDownloadedData)
    IPC_MESSAGE_HANDLER(ResourceMsg_RequestComplete, OnRequestComplete)
  IPC_END_MESSAGE_MAP()
}

void ResourceDispatcher::FlushDeferredMessages(int request_id) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info || request_info->is_deferred)
    return;

  if (request_info->url_loader) {
    DCHECK(request_info->deferred_message_queue.empty());
    request_info->url_loader_client->FlushDeferredMessages();
    return;
  }

  // Because message handlers could result in request_info being destroyed,
  // we need to work with a stack reference to the deferred queue.
  MessageQueue q;
  q.swap(request_info->deferred_message_queue);
  while (!q.empty()) {
    IPC::Message* m = q.front();
    q.pop_front();
    DispatchMessage(*m);
    delete m;
    // We need to find the request again in the list as it may have completed
    // by now and the request_info instance above may be invalid.
    request_info = GetPendingRequestInfo(request_id);
    if (!request_info) {
      // The recipient is gone, the messages won't be handled and
      // resources they might hold won't be released. Explicitly release
      // them from here so that they won't leak.
      ReleaseResourcesInMessageQueue(&q);
      return;
    }
    // If this request is deferred in the context of the above message, then
    // we should honor the same and stop dispatching further messages.
    if (request_info->is_deferred) {
      request_info->deferred_message_queue.swap(q);
      return;
    }
  }
}

void ResourceDispatcher::StartSync(
    std::unique_ptr<ResourceRequest> request,
    int routing_id,
    SyncLoadResponse* response,
    blink::WebURLRequest::LoadingIPCType ipc_type,
    mojom::URLLoaderFactory* url_loader_factory) {
  CheckSchemeForReferrerPolicy(*request);

  SyncLoadResult result;

  if (ipc_type == blink::WebURLRequest::LoadingIPCType::kMojo) {
    if (!url_loader_factory->SyncLoad(
            routing_id, MakeRequestID(), *request, &result)) {
      response->error_code = net::ERR_FAILED;
      return;
    }
  } else {
    IPC::SyncMessage* msg = new ResourceHostMsg_SyncLoad(
        routing_id, MakeRequestID(), *request, &result);

    // NOTE: This may pump events (see RenderThread::Send).
    if (!message_sender_->Send(msg)) {
      response->error_code = net::ERR_FAILED;
      return;
    }
  }

  response->error_code = result.error_code;
  response->url = result.final_url;
  response->headers = result.headers;
  response->mime_type = result.mime_type;
  response->charset = result.charset;
  response->request_time = result.request_time;
  response->response_time = result.response_time;
  response->load_timing = result.load_timing;
  response->devtools_info = result.devtools_info;
  response->data.swap(result.data);
  response->download_file_path = result.download_file_path;
  response->socket_address = result.socket_address;
  response->encoded_data_length = result.encoded_data_length;
  response->encoded_body_length = result.encoded_body_length;
}

int ResourceDispatcher::StartAsync(
    std::unique_ptr<ResourceRequest> request,
    int routing_id,
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
    const url::Origin& frame_origin,
    std::unique_ptr<RequestPeer> peer,
    blink::WebURLRequest::LoadingIPCType ipc_type,
    mojom::URLLoaderFactory* url_loader_factory,
    mojo::ScopedDataPipeConsumerHandle consumer_handle) {
  CheckSchemeForReferrerPolicy(*request);

  // Compute a unique request_id for this renderer process.
  int request_id = MakeRequestID();
  pending_requests_[request_id] = base::MakeUnique<PendingRequestInfo>(
      std::move(peer), request->resource_type, request->origin_pid,
      frame_origin, request->url, request->download_to_file);

  if (resource_scheduling_filter_.get() && loading_task_runner) {
    resource_scheduling_filter_->SetRequestIdTaskRunner(request_id,
                                                        loading_task_runner);
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      loading_task_runner ? loading_task_runner : thread_task_runner_;

  if (consumer_handle.is_valid()) {
    pending_requests_[request_id]->url_loader_client =
        base::MakeUnique<URLLoaderClientImpl>(request_id, this, task_runner);

    task_runner->PostTask(FROM_HERE,
                          base::Bind(&ResourceDispatcher::ContinueForNavigation,
                                     weak_factory_.GetWeakPtr(), request_id,
                                     base::Passed(std::move(consumer_handle))));

    return request_id;
  }

  if (ipc_type == blink::WebURLRequest::LoadingIPCType::kMojo) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        loading_task_runner ? loading_task_runner : thread_task_runner_;
    std::unique_ptr<URLLoaderClientImpl> client(
        new URLLoaderClientImpl(request_id, this, std::move(task_runner)));
    mojom::URLLoaderAssociatedPtr url_loader;
    mojom::URLLoaderClientPtr client_ptr;
    client->Bind(&client_ptr);
    url_loader_factory->CreateLoaderAndStart(
        MakeRequest(&url_loader), routing_id, request_id,
        mojom::kURLLoadOptionNone, *request, std::move(client_ptr));
    pending_requests_[request_id]->url_loader = std::move(url_loader);
    pending_requests_[request_id]->url_loader_client = std::move(client);
  } else {
    message_sender_->Send(
        new ResourceHostMsg_RequestResource(routing_id, request_id, *request));
  }

  return request_id;
}

void ResourceDispatcher::ToResourceResponseInfo(
    const PendingRequestInfo& request_info,
    const ResourceResponseHead& browser_info,
    ResourceResponseInfo* renderer_info) const {
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

  // Collect UMA on the inter-process skew.
  bool is_skew_additive = false;
  if (converter.IsSkewAdditiveForMetrics()) {
    is_skew_additive = true;
    base::TimeDelta skew = converter.GetSkewForMetrics();
    if (skew >= base::TimeDelta()) {
      UMA_HISTOGRAM_TIMES(
          "InterProcessTimeTicks.BrowserAhead_BrowserToRenderer", skew);
    } else {
      UMA_HISTOGRAM_TIMES(
          "InterProcessTimeTicks.BrowserBehind_BrowserToRenderer", -skew);
    }
  }
  UMA_HISTOGRAM_BOOLEAN(
      "InterProcessTimeTicks.IsSkewAdditive_BrowserToRenderer",
      is_skew_additive);
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

base::TimeTicks ResourceDispatcher::ConsumeIOTimestamp() {
  if (io_timestamp_ == base::TimeTicks())
    return base::TimeTicks::Now();
  base::TimeTicks result = io_timestamp_;
  io_timestamp_ = base::TimeTicks();
  return result;
}

void ResourceDispatcher::ContinueForNavigation(
    int request_id,
    mojo::ScopedDataPipeConsumerHandle consumer_handle) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  URLLoaderClientImpl* client_ptr = request_info->url_loader_client.get();

  // Short circuiting call to OnReceivedResponse to immediately start
  // the request. ResourceResponseHead can be empty here because we
  // pull the StreamOverride's one in
  // WebURLLoaderImpl::Context::OnReceivedResponse.
  client_ptr->OnReceiveResponse(ResourceResponseHead(), base::nullopt,
                                mojom::DownloadedTempFilePtr());
  // Start streaming now.
  client_ptr->OnStartLoadingResponseBody(std::move(consumer_handle));

  // Call OnComplete now too, as it won't get called on the client.
  // TODO(kinuko): Fill this properly.
  ResourceRequestCompletionStatus completion_status;
  completion_status.error_code = net::OK;
  completion_status.was_ignored_by_handler = false;
  completion_status.exists_in_cache = false;
  completion_status.completion_time = base::TimeTicks::Now();
  completion_status.encoded_data_length = -1;
  completion_status.encoded_body_length = -1;
  completion_status.decoded_body_length = -1;
  client_ptr->OnComplete(completion_status);
}

// static
bool ResourceDispatcher::IsResourceDispatcherMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case ResourceMsg_UploadProgress::ID:
    case ResourceMsg_ReceivedResponse::ID:
    case ResourceMsg_ReceivedCachedMetadata::ID:
    case ResourceMsg_ReceivedRedirect::ID:
    case ResourceMsg_SetDataBuffer::ID:
    case ResourceMsg_DataReceived::ID:
    case ResourceMsg_DataDownloaded::ID:
    case ResourceMsg_RequestComplete::ID:
      return true;

    default:
      break;
  }

  return false;
}

// static
void ResourceDispatcher::ReleaseResourcesInDataMessage(
    const IPC::Message& message) {
  base::PickleIterator iter(message);
  int request_id;
  if (!iter.ReadInt(&request_id)) {
    NOTREACHED() << "malformed resource message";
    return;
  }

  // If the message contains a shared memory handle, we should close the handle
  // or there will be a memory leak.
  if (message.type() == ResourceMsg_SetDataBuffer::ID) {
    base::SharedMemoryHandle shm_handle;
    if (IPC::ParamTraits<base::SharedMemoryHandle>::Read(&message,
                                                         &iter,
                                                         &shm_handle)) {
      if (base::SharedMemory::IsHandleValid(shm_handle))
        base::SharedMemory::CloseHandle(shm_handle);
    }
  }
}

// static
void ResourceDispatcher::ReleaseResourcesInMessageQueue(MessageQueue* queue) {
  while (!queue->empty()) {
    IPC::Message* message = queue->front();
    ReleaseResourcesInDataMessage(*message);
    queue->pop_front();
    delete message;
  }
}

void ResourceDispatcher::SetResourceSchedulingFilter(
    scoped_refptr<ResourceSchedulingFilter> resource_scheduling_filter) {
  resource_scheduling_filter_ = resource_scheduling_filter;
}

}  // namespace content
