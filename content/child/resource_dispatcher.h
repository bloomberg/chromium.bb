// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#ifndef CONTENT_CHILD_RESOURCE_DISPATCHER_H_
#define CONTENT_CHILD_RESOURCE_DISPATCHER_H_

#include <stdint.h>

#include <deque>
#include <map>
#include <memory>
#include <string>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/url_loader.mojom.h"
#include "content/public/common/url_loader_throttle.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/request_priority.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
struct RedirectInfo;
}

namespace content {
class RequestPeer;
class ResourceDispatcherDelegate;
class ResourceSchedulingFilter;
struct ResourceResponseInfo;
struct ResourceRequest;
struct ResourceRequestCompletionStatus;
struct ResourceResponseHead;
class SharedMemoryReceivedDataFactory;
struct SiteIsolationResponseMetaData;
struct SyncLoadResponse;
class ThrottlingURLLoader;
class URLLoaderClientImpl;

namespace mojom {
class URLLoaderFactory;
}  // namespace mojom

// This class serves as a communication interface to the ResourceDispatcherHost
// in the browser process. It can be used from any child process.
// Virtual methods are for tests.
class CONTENT_EXPORT ResourceDispatcher : public IPC::Listener {
 public:
  ResourceDispatcher(
      IPC::Sender* sender,
      scoped_refptr<base::SingleThreadTaskRunner> thread_task_runner);
  ~ResourceDispatcher() override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // Call this method to load the resource synchronously (i.e., in one shot).
  // This is an alternative to the StartAsync method. Be warned that this method
  // will block the calling thread until the resource is fully downloaded or an
  // error occurs. It could block the calling thread for a long time, so only
  // use this if you really need it!  There is also no way for the caller to
  // interrupt this method. Errors are reported via the status field of the
  // response parameter.
  //
  // |routing_id| is used to associated the bridge with a frame's network
  // context.
  virtual void StartSync(
      std::unique_ptr<ResourceRequest> request,
      int routing_id,
      SyncLoadResponse* response,
      blink::WebURLRequest::LoadingIPCType ipc_type,
      mojom::URLLoaderFactory* url_loader_factory,
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles);

  // Call this method to initiate the request. If this method succeeds, then
  // the peer's methods will be called asynchronously to report various events.
  // Returns the request id. |url_loader_factory| must be non-null if and only
  // if |ipc_type| is LoadingIPCType::Mojo.
  //
  // |routing_id| is used to associated the bridge with a frame's network
  // context.
  //
  // You can pass an optional argument |loading_task_runner| to specify task
  // queue to execute loading tasks on.
  virtual int StartAsync(
      std::unique_ptr<ResourceRequest> request,
      int routing_id,
      scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
      const url::Origin& frame_origin,
      std::unique_ptr<RequestPeer> peer,
      blink::WebURLRequest::LoadingIPCType ipc_type,
      mojom::URLLoaderFactory* url_loader_factory,
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      mojo::ScopedDataPipeConsumerHandle consumer_handle);

  // Removes a request from the |pending_requests_| list, returning true if the
  // request was found and removed.
  bool RemovePendingRequest(int request_id);

  // Cancels a request in the |pending_requests_| list.  The request will be
  // removed from the dispatcher as well.
  virtual void Cancel(int request_id);

  // Toggles the is_deferred attribute for the specified request.
  virtual void SetDefersLoading(int request_id, bool value);

  // Indicates the priority of the specified request changed.
  void DidChangePriority(int request_id,
                         net::RequestPriority new_priority,
                         int intra_priority_value);

  void set_message_sender(IPC::Sender* sender) {
    DCHECK(sender);
    DCHECK(pending_requests_.empty());
    message_sender_ = sender;
  }

  // This does not take ownership of the delegate. It is expected that the
  // delegate have a longer lifetime than the ResourceDispatcher.
  void set_delegate(ResourceDispatcherDelegate* delegate) {
    delegate_ = delegate;
  }

  // Remembers IO thread timestamp for next resource message.
  void set_io_timestamp(base::TimeTicks io_timestamp) {
    io_timestamp_ = io_timestamp;
  }

  void SetThreadTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> thread_task_runner) {
    thread_task_runner_ = thread_task_runner;
  }

  void SetResourceSchedulingFilter(
      scoped_refptr<ResourceSchedulingFilter> resource_scheduling_filter);

  base::WeakPtr<ResourceDispatcher> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void OnTransferSizeUpdated(int request_id, int32_t transfer_size_diff);

 private:
  friend class URLLoaderClientImpl;
  friend class URLResponseBodyConsumer;
  friend class ResourceDispatcherTest;

  typedef std::deque<IPC::Message*> MessageQueue;
  struct PendingRequestInfo {
    PendingRequestInfo(std::unique_ptr<RequestPeer> peer,
                       ResourceType resource_type,
                       int origin_pid,
                       const url::Origin& frame_origin,
                       const GURL& request_url,
                       bool download_to_file);

    ~PendingRequestInfo();

    std::unique_ptr<RequestPeer> peer;
    ResourceType resource_type;
    // The PID of the original process which issued this request. This gets
    // non-zero only for a request proxied by another renderer, particularly
    // requests from plugins.
    int origin_pid;
    MessageQueue deferred_message_queue;
    bool is_deferred = false;
    // Original requested url.
    GURL url;
    // The security origin of the frame that initiates this request.
    url::Origin frame_origin;
    // The url of the latest response even in case of redirection.
    GURL response_url;
    bool download_to_file;
    std::unique_ptr<IPC::Message> pending_redirect_message;
    base::TimeTicks request_start;
    base::TimeTicks response_start;
    base::TimeTicks completion_time;
    linked_ptr<base::SharedMemory> buffer;
    scoped_refptr<SharedMemoryReceivedDataFactory> received_data_factory;
    std::unique_ptr<SiteIsolationResponseMetaData> site_isolation_metadata;
    int buffer_size;

    // For mojo loading.
    std::unique_ptr<ThrottlingURLLoader> url_loader;
    std::unique_ptr<URLLoaderClientImpl> url_loader_client;
  };
  using PendingRequestMap = std::map<int, std::unique_ptr<PendingRequestInfo>>;

  // Helper to lookup the info based on the request_id.
  // May return NULL if the request as been canceled from the client side.
  PendingRequestInfo* GetPendingRequestInfo(int request_id);

  // Follows redirect, if any, for the given request.
  void FollowPendingRedirect(int request_id, PendingRequestInfo* request_info);

  // Message response handlers, called by the message handler for this process.
  void OnUploadProgress(int request_id, int64_t position, int64_t size);
  void OnReceivedResponse(int request_id, const ResourceResponseHead&);
  void OnReceivedCachedMetadata(int request_id,
                                const std::vector<uint8_t>& data);
  void OnReceivedRedirect(int request_id,
                          const net::RedirectInfo& redirect_info,
                          const ResourceResponseHead& response_head);
  void OnSetDataBuffer(int request_id,
                       base::SharedMemoryHandle shm_handle,
                       int shm_size,
                       base::ProcessId renderer_pid);
  void OnReceivedData(int request_id,
                      int data_offset,
                      int data_length,
                      int encoded_data_length);
  void OnDownloadedData(int request_id, int data_len, int encoded_data_length);
  void OnRequestComplete(
      int request_id,
      const ResourceRequestCompletionStatus& request_complete_data);

  // Dispatch the message to one of the message response handlers.
  void DispatchMessage(const IPC::Message& message);

  // Dispatch any deferred messages for the given request, provided it is not
  // again in the deferred state. This method may mutate |pending_requests_|.
  void FlushDeferredMessages(int request_id);

  void ToResourceResponseInfo(const PendingRequestInfo& request_info,
                              const ResourceResponseHead& browser_info,
                              ResourceResponseInfo* renderer_info) const;

  base::TimeTicks ToRendererCompletionTime(
      const PendingRequestInfo& request_info,
      const base::TimeTicks& browser_completion_time) const;

  // Returns timestamp provided by IO thread. If no timestamp is supplied,
  // then current time is returned. Saved timestamp is reset, so following
  // invocations will return current time until set_io_timestamp is called.
  base::TimeTicks ConsumeIOTimestamp();

  void ContinueForNavigation(
      int request_id,
      mojo::ScopedDataPipeConsumerHandle consumer_handle);

  // Returns true if the message passed in is a resource related message.
  static bool IsResourceDispatcherMessage(const IPC::Message& message);

  // ViewHostMsg_Resource_DataReceived is not POD, it has a shared memory
  // handle in it that we should cleanup it up nicely. This method accepts any
  // message and determine whether the message is
  // ViewHostMsg_Resource_DataReceived and clean up the shared memory handle.
  static void ReleaseResourcesInDataMessage(const IPC::Message& message);

  // Iterate through a message queue and clean up the messages by calling
  // ReleaseResourcesInDataMessage and removing them from the queue. Intended
  // for use on deferred message queues that are no longer needed.
  static void ReleaseResourcesInMessageQueue(MessageQueue* queue);

  IPC::Sender* message_sender_;

  // All pending requests issued to the host
  PendingRequestMap pending_requests_;

  ResourceDispatcherDelegate* delegate_;

  // IO thread timestamp for ongoing IPC message.
  base::TimeTicks io_timestamp_;

  scoped_refptr<base::SingleThreadTaskRunner> thread_task_runner_;
  scoped_refptr<ResourceSchedulingFilter> resource_scheduling_filter_;

  base::WeakPtrFactory<ResourceDispatcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourceDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_RESOURCE_DISPATCHER_H_
