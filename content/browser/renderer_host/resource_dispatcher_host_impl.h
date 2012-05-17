// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the browser side of the resource dispatcher, it receives requests
// from the child process (i.e. [Renderer, Plugin, Worker]ProcessHost), and
// dispatches them to URLRequests. It then forwards the messages from the
// URLRequests back to the correct process for handling.
//
// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#ifndef CONTENT_BROWSER_RENDERER_HOST_RESOURCE_DISPATCHER_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RESOURCE_DISPATCHER_HOST_IMPL_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/common/content_export.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "ipc/ipc_message.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/resource_type.h"

class DownloadFileManager;
class ResourceHandler;
class ResourceMessageFilter;
class SaveFileManager;
class WebContentsImpl;
struct ResourceHostMsg_Request;
struct ViewMsg_SwapOut_Params;

namespace net {
class CookieList;
class URLRequestJobFactory;
}

namespace webkit_blob {
class ShareableFileReference;
}

namespace content {
class ResourceContext;
class ResourceDispatcherHostDelegate;
class ResourceRequestInfoImpl;
struct DownloadSaveInfo;
struct GlobalRequestID;
struct Referrer;

class CONTENT_EXPORT ResourceDispatcherHostImpl
    : public ResourceDispatcherHost,
      public net::URLRequest::Delegate,
      public SSLErrorHandler::Delegate {
 public:
  ResourceDispatcherHostImpl();
  virtual ~ResourceDispatcherHostImpl();

  // Returns the current ResourceDispatcherHostImpl. May return NULL if it
  // hasn't been created yet.
  static ResourceDispatcherHostImpl* Get();

  // ResourceDispatcherHost implementation:
  virtual void SetDelegate(ResourceDispatcherHostDelegate* delegate) OVERRIDE;
  virtual void SetAllowCrossOriginAuthPrompt(bool value) OVERRIDE;
  virtual net::Error BeginDownload(
      scoped_ptr<net::URLRequest> request,
      bool is_content_initiated,
      ResourceContext* context,
      int child_id,
      int route_id,
      bool prefer_cache,
      const DownloadSaveInfo& save_info,
      const DownloadStartedCallback& started_callback) OVERRIDE;
  virtual void ClearLoginDelegateForRequest(net::URLRequest* request) OVERRIDE;
  virtual void MarkAsTransferredNavigation(net::URLRequest* request) OVERRIDE;

  // Puts the resource dispatcher host in an inactive state (unable to begin
  // new requests).  Cancels all pending requests.
  void Shutdown();

  // Notify the ResourceDispatcherHostImpl of a new resource context.
  void AddResourceContext(ResourceContext* context);

  // Notify the ResourceDispatcherHostImpl of a resource context destruction.
  void RemoveResourceContext(ResourceContext* context);

  // Force cancels any pending requests for the given |context|. This is
  // necessary to ensure that before |context| goes away, all requests
  // for it are dead.
  void CancelRequestsForContext(ResourceContext* context);

  // Returns true if the message was a resource message that was processed.
  // If it was, message_was_ok will be false iff the message was corrupt.
  bool OnMessageReceived(const IPC::Message& message,
                         ResourceMessageFilter* filter,
                         bool* message_was_ok);

  // Initiates a save file from the browser process (as opposed to a resource
  // request from the renderer or another child process).
  void BeginSaveFile(const GURL& url,
                     const content::Referrer& referrer,
                     int child_id,
                     int route_id,
                     ResourceContext* context);

  // Cancels the given request if it still exists. We ignore cancels from the
  // renderer in the event of a download.
  void CancelRequest(int child_id,
                     int request_id,
                     bool from_renderer);

  // Follows a deferred redirect for the given request.
  // new_first_party_for_cookies, if non-empty, is the new cookie policy URL
  // for the redirected URL.  If the cookie policy URL needs changing, pass
  // true as has_new_first_party_for_cookies and the new cookie policy URL as
  // new_first_party_for_cookies.  Otherwise, pass false as
  // has_new_first_party_for_cookies, and new_first_party_for_cookies will not
  // be used.
  void FollowDeferredRedirect(int child_id,
                              int request_id,
                              bool has_new_first_party_for_cookies,
                              const GURL& new_first_party_for_cookies);

  // Starts a request that was deferred during ResourceHandler::OnWillStart().
  void StartDeferredRequest(int child_id, int request_id);

  // Returns true if it's ok to send the data. If there are already too many
  // data messages pending, it pauses the request and returns false. In this
  // case the caller should not send the data.
  bool WillSendData(int child_id, int request_id);

  // Pauses or resumes network activity for a particular request.
  void PauseRequest(int child_id, int request_id, bool pause);

  // Returns the number of pending requests. This is designed for the unittests
  int pending_requests() const {
    return static_cast<int>(pending_requests_.size());
  }

  // Intended for unit-tests only. Returns the memory cost of all the
  // outstanding requests (pending and blocked) for |child_id|.
  int GetOutstandingRequestsMemoryCost(int child_id) const;

  // Intended for unit-tests only. Overrides the outstanding requests bound.
  void set_max_outstanding_requests_cost_per_process(int limit) {
    max_outstanding_requests_cost_per_process_ = limit;
  }

  // The average private bytes increase of the browser for each new pending
  // request. Experimentally obtained.
  static const int kAvgBytesPerOutstandingRequest = 4400;

  DownloadFileManager* download_file_manager() const {
    return download_file_manager_;
  }

  SaveFileManager* save_file_manager() const {
    return save_file_manager_;
  }

  // Called when the unload handler for a cross-site request has finished.
  void OnSwapOutACK(const ViewMsg_SwapOut_Params& params);

  // Called when the renderer loads a resource from its internal cache.
  void OnDidLoadResourceFromMemoryCache(const GURL& url,
                                        const std::string& security_info,
                                        const std::string& http_method,
                                        ResourceType::Type resource_type);

  // Force cancels any pending requests for the given process.
  void CancelRequestsForProcess(int child_id);

  // Force cancels any pending requests for the given route id.  This method
  // acts like CancelRequestsForProcess when route_id is -1.
  void CancelRequestsForRoute(int child_id, int route_id);

  // net::URLRequest::Delegate:
  virtual void OnReceivedRedirect(net::URLRequest* request,
                                  const GURL& new_url,
                                  bool* defer_redirect) OVERRIDE;
  virtual void OnAuthRequired(net::URLRequest* request,
                              net::AuthChallengeInfo* auth_info) OVERRIDE;
  virtual void OnCertificateRequested(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info) OVERRIDE;
  virtual void OnSSLCertificateError(net::URLRequest* request,
                                     const net::SSLInfo& ssl_info,
                                     bool fatal) OVERRIDE;
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE;

  // SSLErrorHandler::Delegate:
  virtual void CancelSSLRequest(const content::GlobalRequestID& id,
                                int error,
                                const net::SSLInfo* ssl_info) OVERRIDE;
  virtual void ContinueSSLRequest(const content::GlobalRequestID& id) OVERRIDE;

  void OnUserGesture(WebContentsImpl* contents);

  // Retrieves a net::URLRequest.  Must be called from the IO thread.
  net::URLRequest* GetURLRequest(
      const GlobalRequestID& request_id) const;

  void RemovePendingRequest(int child_id, int request_id);

  // Causes all new requests for the route identified by
  // |child_id| and |route_id| to be blocked (not being
  // started) until ResumeBlockedRequestsForRoute or
  // CancelBlockedRequestsForRoute is called.
  void BlockRequestsForRoute(int child_id, int route_id);

  // Resumes any blocked request for the specified route id.
  void ResumeBlockedRequestsForRoute(int child_id, int route_id);

  // Cancels any blocked request for the specified route id.
  void CancelBlockedRequestsForRoute(int child_id, int route_id);

  // Decrements the pending_data_count for the request and resumes
  // the request if it was paused due to too many pending data
  // messages sent.
  void DataReceivedACK(int child_id, int request_id);

  // Maintains a collection of temp files created in support of
  // the download_to_file capability. Used to grant access to the
  // child process and to defer deletion of the file until it's
  // no longer needed.
  void RegisterDownloadedTempFile(
      int child_id, int request_id,
      webkit_blob::ShareableFileReference* reference);
  void UnregisterDownloadedTempFile(int child_id, int request_id);

  // Needed for the sync IPC message dispatcher macros.
  bool Send(IPC::Message* message);

  // Indicates whether third-party sub-content can pop-up HTTP basic auth
  // dialog boxes.
  bool allow_cross_origin_auth_prompt();

  ResourceDispatcherHostDelegate* delegate() {
    return delegate_;
  }

  scoped_refptr<ResourceHandler> CreateResourceHandlerForDownload(
      net::URLRequest* request,
      ResourceContext* context,
      int child_id,
      int route_id,
      int request_id,
      bool is_content_initiated,
      const DownloadSaveInfo& save_info,
      const DownloadResourceHandler::OnStartedCallback& started_cb);

 private:
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest,
                           TestBlockedRequestsProcessDies);
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest,
                           IncrementOutstandingRequestsMemoryCost);
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest,
                           CalculateApproximateMemoryCost);

  class ShutdownTask;

  friend class ShutdownTask;

  // Extracts the render view/process host's identifiers from the given request
  // and places them in the given out params (both required). If there are no
  // such IDs associated with the request (such as non-page-related requests),
  // this function will return false and both out params will be -1.
  static bool RenderViewForRequest(const net::URLRequest* request,
                                   int* render_process_host_id,
                                   int* render_view_host_id);

  // A shutdown helper that runs on the IO thread.
  void OnShutdown();

  void StartRequest(net::URLRequest* request);

  // Returns true if the request is paused.
  bool PauseRequestIfNeeded(ResourceRequestInfoImpl* info);

  // Resumes the given request by calling OnResponseStarted or OnReadCompleted.
  void ResumeRequest(const GlobalRequestID& request_id);

  // Internal function to start reading for the first time.
  void StartReading(net::URLRequest* request);

  // Reads data from the response using our internal buffer as async IO.
  // Returns true if data is available immediately, false otherwise.  If the
  // return value is false, we will receive a OnReadComplete() callback later.
  bool Read(net::URLRequest* request, int* bytes_read);

  // Internal function to finish an async IO which has completed.  Returns
  // true if there is more data to read (e.g. we haven't read EOF yet and
  // no errors have occurred).
  bool CompleteRead(net::URLRequest*, int* bytes_read);

  // Internal function to finish handling the ResponseStarted message.  Returns
  // true on success.
  bool CompleteResponseStarted(net::URLRequest* request);

  void ResponseCompleted(net::URLRequest* request);
  void CallResponseCompleted(int child_id, int request_id);

  // Helper function for regular and download requests.
  void BeginRequestInternal(net::URLRequest* request);

  // Helper function that cancels |request|.  Returns whether the
  // request was actually cancelled.  If a renderer cancels a request
  // for a download, we ignore the cancellation.
  bool CancelRequestInternal(net::URLRequest* request, bool from_renderer);

  // Updates the "cost" of outstanding requests for |child_id|.
  // The "cost" approximates how many bytes are consumed by all the in-memory
  // data structures supporting this request (net::URLRequest object,
  // HttpNetworkTransaction, etc...).
  // The value of |cost| is added to the running total, and the resulting
  // sum is returned.
  int IncrementOutstandingRequestsMemoryCost(int cost,
                                             int child_id);

  // Estimate how much heap space |request| will consume to run.
  static int CalculateApproximateMemoryCost(net::URLRequest* request);

  // The list of all requests that we have pending. This list is not really
  // optimized, and assumes that we have relatively few requests pending at once
  // since some operations require brute-force searching of the list.
  //
  // It may be enhanced in the future to provide some kind of prioritization
  // mechanism. We should also consider a hashtable or binary tree if it turns
  // out we have a lot of things here.
  typedef std::map<GlobalRequestID, net::URLRequest*>
      PendingRequestList;

  // Deletes the pending request identified by the iterator passed in.
  // This function will invalidate the iterator passed in. Callers should
  // not rely on this iterator being valid on return.
  void RemovePendingRequest(const PendingRequestList::iterator& iter);

  // Notify our observers that we started receiving a response for a request.
  void NotifyResponseStarted(net::URLRequest* request, int child_id);

  // Notify our observers that a request has been redirected.
  void NotifyReceivedRedirect(net::URLRequest* request,
                              int child_id,
                              const GURL& new_url);

  // Tries to handle the url with an external protocol. If the request is
  // handled, the function returns true. False otherwise.
  bool HandleExternalProtocol(int request_id,
                              int child_id,
                              int route_id,
                              const GURL& url,
                              ResourceType::Type resource_type,
                              const net::URLRequestJobFactory& job_factory,
                              ResourceHandler* handler);

  // Checks all pending requests and updates the load states and upload
  // progress if necessary.
  void UpdateLoadStates();

  // Checks the upload state and sends an update if one is necessary.
  void MaybeUpdateUploadProgress(ResourceRequestInfoImpl *info,
                                 net::URLRequest *request);

  // Resumes or cancels (if |cancel_requests| is true) any blocked requests.
  void ProcessBlockedRequestsForRoute(int child_id,
                                      int route_id,
                                      bool cancel_requests);

  void OnRequestResource(const IPC::Message& msg,
                         int request_id,
                         const ResourceHostMsg_Request& request_data);
  void OnSyncLoad(int request_id,
                  const ResourceHostMsg_Request& request_data,
                  IPC::Message* sync_result);
  void BeginRequest(int request_id,
                    const ResourceHostMsg_Request& request_data,
                    IPC::Message* sync_result,  // only valid for sync
                    int route_id);  // only valid for async
  void OnDataReceivedACK(int request_id);
  void OnDataDownloadedACK(int request_id);
  void OnUploadProgressACK(int request_id);
  void OnCancelRequest(int request_id);
  void OnTransferRequestToNewPage(int new_routing_id, int request_id);
  void OnFollowRedirect(int request_id,
                        bool has_new_first_party_for_cookies,
                        const GURL& new_first_party_for_cookies);
  void OnReleaseDownloadedFile(int request_id);

  // Creates ResourceRequestInfoImpl for a download or page save.
  // |download| should be true if the request is a file download.
  ResourceRequestInfoImpl* CreateRequestInfo(
      ResourceHandler* handler,
      int child_id,
      int route_id,
      bool download,
      ResourceContext* context);

  // Returns true if |request| is in |pending_requests_|.
  bool IsValidRequest(net::URLRequest* request);

  // Sends the given notification on the UI thread.  The RenderViewHost's
  // controller is used as the source.
  template <class T>
  static void NotifyOnUI(int type,
                         int render_process_id,
                         int render_view_id,
                         T* detail);

  // Relationship of resource being authenticated with the top level page.
  enum HttpAuthResourceType {
    HTTP_AUTH_RESOURCE_TOP,            // Top-level page itself
    HTTP_AUTH_RESOURCE_SAME_DOMAIN,    // Sub-content from same domain
    HTTP_AUTH_RESOURCE_BLOCKED_CROSS,  // Blocked Sub-content from cross domain
    HTTP_AUTH_RESOURCE_ALLOWED_CROSS,  // Allowed Sub-content per command line
    HTTP_AUTH_RESOURCE_LAST
  };

  HttpAuthResourceType HttpAuthResourceTypeOf(net::URLRequest* request);

  // Returns whether the URLRequest identified by |transferred_request_id| is
  // currently in the process of being transferred to a different renderer.
  // This happens if a request is redirected cross-site and needs to be resumed
  // by a new render view.
  bool IsTransferredNavigation(
      const GlobalRequestID& transferred_request_id) const;

  PendingRequestList pending_requests_;

  // Collection of temp files downloaded for child processes via
  // the download_to_file mechanism. We avoid deleting them until
  // the client no longer needs them.
  typedef std::map<int, scoped_refptr<webkit_blob::ShareableFileReference> >
      DeletableFilesMap;  // key is request id
  typedef std::map<int, DeletableFilesMap>
      RegisteredTempFiles;  // key is child process id
  RegisteredTempFiles registered_temp_files_;

  // A timer that periodically calls UpdateLoadStates while pending_requests_
  // is not empty.
  scoped_ptr<base::RepeatingTimer<ResourceDispatcherHostImpl> >
      update_load_states_timer_;

  // We own the download file writing thread and manager
  scoped_refptr<DownloadFileManager> download_file_manager_;

  // We own the save file manager.
  scoped_refptr<SaveFileManager> save_file_manager_;

  // Request ID for browser initiated requests. request_ids generated by
  // child processes are counted up from 0, while browser created requests
  // start at -2 and go down from there. (We need to start at -2 because -1 is
  // used as a special value all over the resource_dispatcher_host for
  // uninitialized variables.) This way, we no longer have the unlikely (but
  // observed in the real world!) event where we have two requests with the same
  // request_id_.
  int request_id_;

  // For running tasks.
  base::WeakPtrFactory<ResourceDispatcherHostImpl> weak_factory_;

  // For SSLErrorHandler::Delegate calls from SSLManager.
  base::WeakPtrFactory<SSLErrorHandler::Delegate> ssl_delegate_weak_factory_;

  // True if the resource dispatcher host has been shut down.
  bool is_shutdown_;

  typedef std::vector<net::URLRequest*> BlockedRequestsList;
  typedef std::pair<int, int> ProcessRouteIDs;
  typedef std::map<ProcessRouteIDs, BlockedRequestsList*> BlockedRequestMap;
  BlockedRequestMap blocked_requests_map_;

  // Maps the child_ids to the approximate number of bytes
  // being used to service its resource requests. No entry implies 0 cost.
  typedef std::map<int, int> OutstandingRequestsMemoryCostMap;
  OutstandingRequestsMemoryCostMap outstanding_requests_memory_cost_map_;

  // |max_outstanding_requests_cost_per_process_| is the upper bound on how
  // many outstanding requests can be issued per child process host.
  // The constraint is expressed in terms of bytes (where the cost of
  // individual requests is given by CalculateApproximateMemoryCost).
  // The total number of outstanding requests is roughly:
  //   (max_outstanding_requests_cost_per_process_ /
  //       kAvgBytesPerOutstandingRequest)
  int max_outstanding_requests_cost_per_process_;

  // Time of the last user gesture. Stored so that we can add a load
  // flag to requests occurring soon after a gesture to indicate they
  // may be because of explicit user action.
  base::TimeTicks last_user_gesture_time_;

  // Used during IPC message dispatching so that the handlers can get a pointer
  // to the source of the message.
  ResourceMessageFilter* filter_;

  ResourceDispatcherHostDelegate* delegate_;

  bool allow_cross_origin_auth_prompt_;

  // Maps the request ID of request that is being transferred to a new RVH
  // to the respective request.
  typedef std::map<GlobalRequestID, net::URLRequest*> TransferredNavigations;
  TransferredNavigations transferred_navigations_;

  // http://crbug.com/90971 - Assists in tracking down use-after-frees on
  // shutdown.
  std::set<const ResourceContext*> active_resource_contexts_;

  DISALLOW_COPY_AND_ASSIGN(ResourceDispatcherHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RESOURCE_DISPATCHER_HOST_IMPL_H_
