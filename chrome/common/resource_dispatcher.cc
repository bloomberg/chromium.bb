// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#include "chrome/common/resource_dispatcher.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/shared_memory.h"
#include "base/string_util.h"
#include "chrome/common/extensions/extension_localization_peer.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/resource_response.h"
#include "chrome/common/security_filter_peer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/upload_data.h"
#include "net/http/http_response_headers.h"
#include "webkit/glue/resource_type.h"
#include "webkit/glue/webkit_glue.h"

// Each resource request is assigned an ID scoped to this process.
static int MakeRequestID() {
  // NOTE: The resource_dispatcher_host also needs probably unique
  // request_ids, so they count down from -2 (-1 is a special we're
  // screwed value), while the renderer process counts up.
  static int next_request_id = 0;
  return next_request_id++;
}

// ResourceLoaderBridge implementation ----------------------------------------

namespace webkit_glue {

class IPCResourceLoaderBridge : public ResourceLoaderBridge {
 public:
  IPCResourceLoaderBridge(ResourceDispatcher* dispatcher,
      const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info,
      int host_renderer_id,
      int host_render_view_id);
  virtual ~IPCResourceLoaderBridge();

  // ResourceLoaderBridge
  virtual void AppendDataToUpload(const char* data, int data_len);
  virtual void AppendFileRangeToUpload(
      const FilePath& path,
      uint64 offset,
      uint64 length,
      const base::Time& expected_modification_time);
  virtual void AppendBlobToUpload(const GURL& blob_url);
  virtual void SetUploadIdentifier(int64 identifier);
  virtual bool Start(Peer* peer);
  virtual void Cancel();
  virtual void SetDefersLoading(bool value);
  virtual void SyncLoad(SyncLoadResponse* response);

 private:
  ResourceLoaderBridge::Peer* peer_;

  // The resource dispatcher for this loader.  The bridge doesn't own it, but
  // it's guaranteed to outlive the bridge.
  ResourceDispatcher* dispatcher_;

  // The request to send, created on initialization for modification and
  // appending data.
  ViewHostMsg_Resource_Request request_;

  // ID for the request, valid once Start()ed, -1 if not valid yet.
  int request_id_;

  // The routing id used when sending IPC messages.
  int routing_id_;

  // The following two members are specified if the request is initiated by
  // a plugin like Gears.

  // Contains the id of the host renderer.
  int host_renderer_id_;

  // Contains the id of the host render view.
  int host_render_view_id_;
};

IPCResourceLoaderBridge::IPCResourceLoaderBridge(
    ResourceDispatcher* dispatcher,
    const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info,
    int host_renderer_id,
    int host_render_view_id)
    : peer_(NULL),
      dispatcher_(dispatcher),
      request_id_(-1),
      routing_id_(request_info.routing_id),
      host_renderer_id_(host_renderer_id),
      host_render_view_id_(host_render_view_id) {
  DCHECK(dispatcher_) << "no resource dispatcher";
  request_.method = request_info.method;
  request_.url = request_info.url;
  request_.first_party_for_cookies = request_info.first_party_for_cookies;
  request_.referrer = request_info.referrer;
  request_.headers = request_info.headers;
  request_.load_flags = request_info.load_flags;
  request_.origin_pid = request_info.requestor_pid;
  request_.resource_type = request_info.request_type;
  request_.request_context = request_info.request_context;
  request_.appcache_host_id = request_info.appcache_host_id;
  request_.download_to_file = request_info.download_to_file;
  request_.has_user_gesture = request_info.has_user_gesture;
  request_.host_renderer_id = host_renderer_id_;
  request_.host_render_view_id = host_render_view_id_;
}

IPCResourceLoaderBridge::~IPCResourceLoaderBridge() {
  // we remove our hook for the resource dispatcher only when going away, since
  // it doesn't keep track of whether we've force terminated the request
  if (request_id_ >= 0) {
    // this operation may fail, as the dispatcher will have preemptively
    // removed us when the renderer sends the ReceivedAllData message.
    dispatcher_->RemovePendingRequest(request_id_);

    if (request_.download_to_file) {
      dispatcher_->message_sender()->Send(
          new ViewHostMsg_ReleaseDownloadedFile(request_id_));
    }
  }
}

void IPCResourceLoaderBridge::AppendDataToUpload(const char* data,
                                                 int data_len) {
  DCHECK(request_id_ == -1) << "request already started";

  // don't bother appending empty data segments
  if (data_len == 0)
    return;

  if (!request_.upload_data)
    request_.upload_data = new net::UploadData();
  request_.upload_data->AppendBytes(data, data_len);
}

void IPCResourceLoaderBridge::AppendFileRangeToUpload(
    const FilePath& path, uint64 offset, uint64 length,
    const base::Time& expected_modification_time) {
  DCHECK(request_id_ == -1) << "request already started";

  if (!request_.upload_data)
    request_.upload_data = new net::UploadData();
  request_.upload_data->AppendFileRange(path, offset, length,
                                        expected_modification_time);
}

void IPCResourceLoaderBridge::AppendBlobToUpload(const GURL& blob_url) {
  DCHECK(request_id_ == -1) << "request already started";

  if (!request_.upload_data)
    request_.upload_data = new net::UploadData();
  request_.upload_data->AppendBlob(blob_url);
}

void IPCResourceLoaderBridge::SetUploadIdentifier(int64 identifier) {
  DCHECK(request_id_ == -1) << "request already started";

  if (!request_.upload_data)
    request_.upload_data = new net::UploadData();
  request_.upload_data->set_identifier(identifier);
}

// Writes a footer on the message and sends it
bool IPCResourceLoaderBridge::Start(Peer* peer) {
  if (request_id_ != -1) {
    NOTREACHED() << "Starting a request twice";
    return false;
  }

  peer_ = peer;

  // generate the request ID, and append it to the message
  request_id_ = dispatcher_->AddPendingRequest(
      peer_, request_.resource_type, request_.url);

  return dispatcher_->message_sender()->Send(
      new ViewHostMsg_RequestResource(routing_id_, request_id_, request_));
}

void IPCResourceLoaderBridge::Cancel() {
  if (request_id_ < 0) {
    NOTREACHED() << "Trying to cancel an unstarted request";
    return;
  }

  dispatcher_->CancelPendingRequest(routing_id_, request_id_);

  // We can't remove the request ID from the resource dispatcher because more
  // data might be pending. Sending the cancel message may cause more data
  // to be flushed, and will then cause a complete message to be sent.
}

void IPCResourceLoaderBridge::SetDefersLoading(bool value) {
  if (request_id_ < 0) {
    NOTREACHED() << "Trying to (un)defer an unstarted request";
    return;
  }

  dispatcher_->SetDefersLoading(request_id_, value);
}

void IPCResourceLoaderBridge::SyncLoad(SyncLoadResponse* response) {
  if (request_id_ != -1) {
    NOTREACHED() << "Starting a request twice";
    response->status.set_status(net::URLRequestStatus::FAILED);
    return;
  }

  request_id_ = MakeRequestID();

  SyncLoadResult result;
  IPC::SyncMessage* msg = new ViewHostMsg_SyncLoad(routing_id_, request_id_,
                                                   request_, &result);
  // NOTE: This may pump events (see RenderThread::Send).
  if (!dispatcher_->message_sender()->Send(msg)) {
    response->status.set_status(net::URLRequestStatus::FAILED);
    return;
  }

  response->status = result.status;
  response->url = result.final_url;
  response->headers = result.headers;
  response->mime_type = result.mime_type;
  response->charset = result.charset;
  response->request_time = result.request_time;
  response->response_time = result.response_time;
  response->connection_id = result.connection_id;
  response->connection_reused = result.connection_reused;
  response->load_timing = result.load_timing;
  response->devtools_info = result.devtools_info;
  response->data.swap(result.data);
  response->download_file_path = result.download_file_path;
}

}  // namespace webkit_glue

// ResourceDispatcher ---------------------------------------------------------

ResourceDispatcher::ResourceDispatcher(IPC::Message::Sender* sender)
    : message_sender_(sender),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

ResourceDispatcher::~ResourceDispatcher() {
}

// ResourceDispatcher implementation ------------------------------------------

bool ResourceDispatcher::OnMessageReceived(const IPC::Message& message) {
  if (!IsResourceDispatcherMessage(message)) {
    return false;
  }

  int request_id;

  void* iter = NULL;
  if (!message.ReadInt(&iter, &request_id)) {
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
    FlushDeferredMessages(request_id);
    // The request could have been deferred now. If yes then the current
    // message has to be queued up. The request_info instance should remain
    // valid here as there are pending messages for it.
    DCHECK(pending_requests_.find(request_id) != pending_requests_.end());
    if (request_info->is_deferred) {
      request_info->deferred_message_queue.push_back(new IPC::Message(message));
      return true;
    }
  }

  DispatchMessage(message);
  return true;
}

ResourceDispatcher::PendingRequestInfo*
ResourceDispatcher::GetPendingRequestInfo(int request_id) {
  PendingRequestList::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    // This might happen for kill()ed requests on the webkit end, so perhaps it
    // shouldn't be a warning...
    DLOG(WARNING) << "Received message for a nonexistent or finished request";
    return NULL;
  }
  return &(it->second);
}

void ResourceDispatcher::OnUploadProgress(
    const IPC::Message& message, int request_id, int64 position, int64 size) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  request_info->peer->OnUploadProgress(position, size);

  // Acknowledge receipt
  message_sender()->Send(
      new ViewHostMsg_UploadProgress_ACK(message.routing_id(), request_id));
}

void ResourceDispatcher::OnReceivedResponse(
    int request_id, const ResourceResponseHead& response_head) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  if (response_head.replace_extension_localization_templates) {
    webkit_glue::ResourceLoaderBridge::Peer* new_peer =
        ExtensionLocalizationPeer::CreateExtensionLocalizationPeer(
            request_info->peer, message_sender(), response_head.mime_type,
            request_info->url);
    if (new_peer)
      request_info->peer = new_peer;
  }

  request_info->peer->OnReceivedResponse(response_head);
}

void ResourceDispatcher::OnReceivedCachedMetadata(
      int request_id, const std::vector<char>& data) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  if (data.size())
    request_info->peer->OnReceivedCachedMetadata(&data.front(), data.size());
}

void ResourceDispatcher::OnReceivedData(const IPC::Message& message,
                                        int request_id,
                                        base::SharedMemoryHandle shm_handle,
                                        int data_len) {
  // Acknowledge the reception of this data.
  message_sender()->Send(
      new ViewHostMsg_DataReceived_ACK(message.routing_id(), request_id));

  const bool shm_valid = base::SharedMemory::IsHandleValid(shm_handle);
  DCHECK((shm_valid && data_len > 0) || (!shm_valid && !data_len));
  base::SharedMemory shared_mem(shm_handle, true);  // read only

  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  if (data_len > 0 && shared_mem.Map(data_len)) {
    const char* data = static_cast<char*>(shared_mem.memory());
    request_info->peer->OnReceivedData(data, data_len);
  }
}

void ResourceDispatcher::OnDownloadedData(const IPC::Message& message,
                                          int request_id,
                                          int data_len) {
  // Acknowledge the reception of this message.
  message_sender()->Send(
      new ViewHostMsg_DataDownloaded_ACK(message.routing_id(), request_id));

  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  request_info->peer->OnDownloadedData(data_len);
}

void ResourceDispatcher::OnReceivedRedirect(
    const IPC::Message& message,
    int request_id,
    const GURL& new_url,
    const webkit_glue::ResourceResponseInfo& info) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  int32 routing_id = message.routing_id();
  bool has_new_first_party_for_cookies = false;
  GURL new_first_party_for_cookies;
  if (request_info->peer->OnReceivedRedirect(new_url, info,
                                            &has_new_first_party_for_cookies,
                                            &new_first_party_for_cookies)) {
    // Double-check if the request is still around. The call above could
    // potentially remove it.
    request_info = GetPendingRequestInfo(request_id);
    if (!request_info)
      return;
    request_info->pending_redirect_message.reset(
        new ViewHostMsg_FollowRedirect(routing_id, request_id,
                                       has_new_first_party_for_cookies,
                                       new_first_party_for_cookies));
    if (!request_info->is_deferred) {
      FollowPendingRedirect(request_id, *request_info);
    }
  } else {
    CancelPendingRequest(routing_id, request_id);
  }
}

void ResourceDispatcher::FollowPendingRedirect(
    int request_id,
    PendingRequestInfo& request_info) {
  IPC::Message* msg = request_info.pending_redirect_message.release();
  if (msg)
    message_sender()->Send(msg);
}

void ResourceDispatcher::OnRequestComplete(int request_id,
                                           const net::URLRequestStatus& status,
                                           const std::string& security_info,
                                           const base::Time& completion_time) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  webkit_glue::ResourceLoaderBridge::Peer* peer = request_info->peer;

  if (status.status() == net::URLRequestStatus::CANCELED &&
      status.os_error() != net::ERR_ABORTED) {
    // Resource canceled with a specific error are filtered.
    SecurityFilterPeer* new_peer =
        SecurityFilterPeer::CreateSecurityFilterPeerForDeniedRequest(
            request_info->resource_type,
            request_info->peer,
            status.os_error());
    if (new_peer) {
      request_info->peer = new_peer;
      peer = new_peer;
    }
  }

  // The request ID will be removed from our pending list in the destructor.
  // Normally, dispatching this message causes the reference-counted request to
  // die immediately.
  peer->OnCompletedRequest(status, security_info, completion_time);

  webkit_glue::NotifyCacheStats();
}

int ResourceDispatcher::AddPendingRequest(
    webkit_glue::ResourceLoaderBridge::Peer* callback,
    ResourceType::Type resource_type,
    const GURL& request_url) {
  // Compute a unique request_id for this renderer process.
  int id = MakeRequestID();
  pending_requests_[id] =
      PendingRequestInfo(callback, resource_type, request_url);
  return id;
}

bool ResourceDispatcher::RemovePendingRequest(int request_id) {
  PendingRequestList::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return false;

  PendingRequestInfo& request_info = it->second;
  ReleaseResourcesInMessageQueue(&request_info.deferred_message_queue);
  pending_requests_.erase(it);

  return true;
}

void ResourceDispatcher::CancelPendingRequest(int routing_id,
                                              int request_id) {
  PendingRequestList::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    DLOG(WARNING) << "unknown request";
    return;
  }

  PendingRequestInfo& request_info = it->second;
  ReleaseResourcesInMessageQueue(&request_info.deferred_message_queue);
  pending_requests_.erase(it);

  message_sender()->Send(
      new ViewHostMsg_CancelRequest(routing_id, request_id));
}

void ResourceDispatcher::SetDefersLoading(int request_id, bool value) {
  PendingRequestList::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    DLOG(ERROR) << "unknown request";
    return;
  }
  PendingRequestInfo& request_info = it->second;
  if (value) {
    request_info.is_deferred = value;
  } else if (request_info.is_deferred) {
    request_info.is_deferred = false;

    FollowPendingRedirect(request_id, request_info);

    MessageLoop::current()->PostTask(FROM_HERE,
        method_factory_.NewRunnableMethod(
            &ResourceDispatcher::FlushDeferredMessages, request_id));
  }
}

void ResourceDispatcher::DispatchMessage(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(ResourceDispatcher, message)
    IPC_MESSAGE_HANDLER(ViewMsg_Resource_UploadProgress, OnUploadProgress)
    IPC_MESSAGE_HANDLER(ViewMsg_Resource_ReceivedResponse, OnReceivedResponse)
    IPC_MESSAGE_HANDLER(
        ViewMsg_Resource_ReceivedCachedMetadata, OnReceivedCachedMetadata)
    IPC_MESSAGE_HANDLER(ViewMsg_Resource_ReceivedRedirect, OnReceivedRedirect)
    IPC_MESSAGE_HANDLER(ViewMsg_Resource_DataReceived, OnReceivedData)
    IPC_MESSAGE_HANDLER(ViewMsg_Resource_DataDownloaded, OnDownloadedData)
    IPC_MESSAGE_HANDLER(ViewMsg_Resource_RequestComplete, OnRequestComplete)
  IPC_END_MESSAGE_MAP()
}

void ResourceDispatcher::FlushDeferredMessages(int request_id) {
  PendingRequestList::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())  // The request could have become invalid.
    return;
  PendingRequestInfo& request_info = it->second;
  if (request_info.is_deferred)
    return;
  // Because message handlers could result in request_info being destroyed,
  // we need to work with a stack reference to the deferred queue.
  MessageQueue q;
  q.swap(request_info.deferred_message_queue);
  while (!q.empty()) {
    IPC::Message* m = q.front();
    q.pop_front();
    DispatchMessage(*m);
    delete m;
    // If this request is deferred in the context of the above message, then
    // we should honor the same and stop dispatching further messages.
    // We need to find the request again in the list as it may have completed
    // by now and the request_info instance above may be invalid.
    PendingRequestList::iterator index = pending_requests_.find(request_id);
    if (index != pending_requests_.end()) {
      PendingRequestInfo& pending_request = index->second;
      if (pending_request.is_deferred) {
        pending_request.deferred_message_queue.swap(q);
        return;
      }
    }
  }
}

webkit_glue::ResourceLoaderBridge* ResourceDispatcher::CreateBridge(
    const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info,
    int host_renderer_id,
    int host_render_view_id) {
  return new webkit_glue::IPCResourceLoaderBridge(this, request_info,
                                                  host_renderer_id,
                                                  host_render_view_id);
}

bool ResourceDispatcher::IsResourceDispatcherMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case ViewMsg_Resource_UploadProgress::ID:
    case ViewMsg_Resource_ReceivedResponse::ID:
    case ViewMsg_Resource_ReceivedCachedMetadata::ID:
    case ViewMsg_Resource_ReceivedRedirect::ID:
    case ViewMsg_Resource_DataReceived::ID:
    case ViewMsg_Resource_DataDownloaded::ID:
    case ViewMsg_Resource_RequestComplete::ID:
      return true;

    default:
      break;
  }

  return false;
}

// static
void ResourceDispatcher::ReleaseResourcesInDataMessage(
    const IPC::Message& message) {
  void* iter = NULL;
  int request_id;
  if (!message.ReadInt(&iter, &request_id)) {
    NOTREACHED() << "malformed resource message";
    return;
  }

  // If the message contains a shared memory handle, we should close the
  // handle or there will be a memory leak.
  if (message.type() == ViewMsg_Resource_DataReceived::ID) {
    base::SharedMemoryHandle shm_handle;
    if (IPC::ParamTraits<base::SharedMemoryHandle>::Read(&message,
                                                         &iter,
                                                         &shm_handle)) {
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
