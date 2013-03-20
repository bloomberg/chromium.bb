// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the browser side of the resource dispatcher, it receives requests
// from the child process (i.e. [Renderer, Plugin, Worker]ProcessHost), and
// dispatches them to URLRequests. It then forwards the messages from the
// URLRequests back to the correct process for handling.
//
// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_DISPATCHER_HOST_IMPL_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_DISPATCHER_HOST_IMPL_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "base/timer.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/loader/render_view_host_tracker.h"
#include "content/browser/loader/resource_loader.h"
#include "content/browser/loader/resource_loader_delegate.h"
#include "content/browser/loader/resource_scheduler.h"
#include "content/common/content_export.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "ipc/ipc_message.h"
#include "net/cookies/canonical_cookie.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/resource_type.h"

class ResourceHandler;
struct ResourceHostMsg_Request;
struct ViewMsg_SwapOut_Params;

namespace net {
class URLRequestJobFactory;
}

namespace webkit_blob {
class ShareableFileReference;
}

namespace content {
class ResourceContext;
class ResourceDispatcherHostDelegate;
class ResourceMessageDelegate;
class ResourceMessageFilter;
class ResourceRequestInfoImpl;
class SaveFileManager;
class WebContentsImpl;
struct DownloadSaveInfo;
struct GlobalRequestID;
struct Referrer;

class CONTENT_EXPORT ResourceDispatcherHostImpl
    : public ResourceDispatcherHost,
      public ResourceLoaderDelegate {
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
      scoped_ptr<DownloadSaveInfo> save_info,
      content::DownloadId download_id,
      const DownloadStartedCallback& started_callback) OVERRIDE;
  virtual void ClearLoginDelegateForRequest(net::URLRequest* request) OVERRIDE;
  virtual void BlockRequestsForRoute(int child_id, int route_id) OVERRIDE;
  virtual void ResumeBlockedRequestsForRoute(
      int child_id, int route_id) OVERRIDE;

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
                     const Referrer& referrer,
                     int child_id,
                     int route_id,
                     ResourceContext* context);

  // Cancels the given request if it still exists. We ignore cancels from the
  // renderer in the event of a download.
  void CancelRequest(int child_id,
                     int request_id,
                     bool from_renderer);

  // Marks the request as "parked". This happens if a request is
  // redirected cross-site and needs to be resumed by a new render view.
  void MarkAsTransferredNavigation(const GlobalRequestID& id);

  // Returns the number of pending requests. This is designed for the unittests
  int pending_requests() const {
    return static_cast<int>(pending_loaders_.size());
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

  SaveFileManager* save_file_manager() const {
    return save_file_manager_;
  }

  // Called when the unload handler for a cross-site request has finished.
  void OnSwapOutACK(const ViewMsg_SwapOut_Params& params);

  // Called when we want to simulate the renderer process sending
  // ViewHostMsg_SwapOut_ACK in cases where the renderer has died or is
  // unresponsive.
  void OnSimulateSwapOutACK(const ViewMsg_SwapOut_Params& params);

  // Called when the renderer loads a resource from its internal cache.
  void OnDidLoadResourceFromMemoryCache(const GURL& url,
                                        const std::string& security_info,
                                        const std::string& http_method,
                                        const std::string& mime_type,
                                        ResourceType::Type resource_type);

  // Called when a RenderViewHost is created.
  void OnRenderViewHostCreated(int child_id, int route_id);

  // Called when a RenderViewHost is deleted.
  void OnRenderViewHostDeleted(int child_id, int route_id);

  // Force cancels any pending requests for the given process.
  void CancelRequestsForProcess(int child_id);

  void OnUserGesture(WebContentsImpl* contents);

  // Retrieves a net::URLRequest.  Must be called from the IO thread.
  net::URLRequest* GetURLRequest(const GlobalRequestID& request_id);

  void RemovePendingRequest(int child_id, int request_id);

  // Cancels any blocked request for the specified route id.
  void CancelBlockedRequestsForRoute(int child_id, int route_id);

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

  // Must be called after the ResourceRequestInfo has been created
  // and associated with the request.
  // |id| should be |DownloadId()| (null) to request automatic
  // assignment.
  scoped_ptr<ResourceHandler> CreateResourceHandlerForDownload(
      net::URLRequest* request,
      bool is_content_initiated,
      bool must_download,
      DownloadId id,
      scoped_ptr<DownloadSaveInfo> save_info,
      const DownloadResourceHandler::OnStartedCallback& started_cb);

  // Must be called after the ResourceRequestInfo has been created
  // and associated with the request.
  scoped_ptr<ResourceHandler> MaybeInterceptAsStream(
      net::URLRequest* request,
      ResourceResponse* response);

  void ClearSSLClientAuthHandlerForRequest(net::URLRequest* request);

  ResourceScheduler* scheduler() { return scheduler_.get(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest,
                           TestBlockedRequestsProcessDies);
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest,
                           IncrementOutstandingRequestsMemoryCost);
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest,
                           CalculateApproximateMemoryCost);

  class ShutdownTask;

  friend class ShutdownTask;
  friend class ResourceMessageDelegate;

  // ResourceLoaderDelegate implementation:
  virtual ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      ResourceLoader* loader,
      net::AuthChallengeInfo* auth_info) OVERRIDE;
  virtual bool AcceptAuthRequest(
      ResourceLoader* loader,
      net::AuthChallengeInfo* auth_info) OVERRIDE;
  virtual bool AcceptSSLClientCertificateRequest(
      ResourceLoader* loader,
      net::SSLCertRequestInfo* cert_info) OVERRIDE;
  virtual bool HandleExternalProtocol(ResourceLoader* loader,
                                      const GURL& url) OVERRIDE;
  virtual void DidStartRequest(ResourceLoader* loader) OVERRIDE;
  virtual void DidReceiveRedirect(ResourceLoader* loader,
                                  const GURL& new_url) OVERRIDE;
  virtual void DidReceiveResponse(ResourceLoader* loader) OVERRIDE;
  virtual void DidFinishLoading(ResourceLoader* loader) OVERRIDE;

  // Extracts the render view/process host's identifiers from the given request
  // and places them in the given out params (both required). If there are no
  // such IDs associated with the request (such as non-page-related requests),
  // this function will return false and both out params will be -1.
  static bool RenderViewForRequest(const net::URLRequest* request,
                                   int* render_process_host_id,
                                   int* render_view_host_id);

  // An init helper that runs on the IO thread.
  void OnInit();

  // A shutdown helper that runs on the IO thread.
  void OnShutdown();

  // The real implementation of the OnSwapOutACK logic. OnSwapOutACK and
  // OnSimulateSwapOutACK just call this method, supplying the |timed_out|
  // parameter, which indicates whether the call is due to a timeout while
  // waiting for SwapOut acknowledgement from the renderer process.
  void HandleSwapOutACK(const ViewMsg_SwapOut_Params& params, bool timed_out);

  // Helper function for regular and download requests.
  void BeginRequestInternal(scoped_ptr<net::URLRequest> request,
                            scoped_ptr<ResourceHandler> handler);

  void StartLoading(ResourceRequestInfoImpl* info,
                    const linked_ptr<ResourceLoader>& loader);

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

  // Force cancels any pending requests for the given route id.  This method
  // acts like CancelRequestsForProcess when route_id is -1.
  void CancelRequestsForRoute(int child_id, int route_id);

  // The list of all requests that we have pending. This list is not really
  // optimized, and assumes that we have relatively few requests pending at once
  // since some operations require brute-force searching of the list.
  //
  // It may be enhanced in the future to provide some kind of prioritization
  // mechanism. We should also consider a hashtable or binary tree if it turns
  // out we have a lot of things here.
  typedef std::map<GlobalRequestID, linked_ptr<ResourceLoader> > LoaderMap;

  // Deletes the pending request identified by the iterator passed in.
  // This function will invalidate the iterator passed in. Callers should
  // not rely on this iterator being valid on return.
  void RemovePendingLoader(const LoaderMap::iterator& iter);

  // Checks all pending requests and updates the load states and upload
  // progress if necessary.
  void UpdateLoadStates();

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
  void OnDataDownloadedACK(int request_id);
  void OnUploadProgressACK(int request_id);
  void OnCancelRequest(int request_id);
  void OnReleaseDownloadedFile(int request_id);

  // Creates ResourceRequestInfoImpl for a download or page save.
  // |download| should be true if the request is a file download.
  ResourceRequestInfoImpl* CreateRequestInfo(
      int child_id,
      int route_id,
      bool download,
      ResourceContext* context);

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

  ResourceLoader* GetLoader(const GlobalRequestID& id) const;
  ResourceLoader* GetLoader(int child_id, int request_id) const;

  // Registers |delegate| to receive resource IPC messages targeted to the
  // specified |id|.
  void RegisterResourceMessageDelegate(const GlobalRequestID& id,
                                       ResourceMessageDelegate* delegate);
  void UnregisterResourceMessageDelegate(const GlobalRequestID& id,
                                         ResourceMessageDelegate* delegate);

  LoaderMap pending_loaders_;

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

  // True if the resource dispatcher host has been shut down.
  bool is_shutdown_;

  typedef std::vector<linked_ptr<ResourceLoader> > BlockedLoadersList;
  typedef std::pair<int, int> ProcessRouteIDs;
  typedef std::map<ProcessRouteIDs, BlockedLoadersList*> BlockedLoadersMap;
  BlockedLoadersMap blocked_loaders_map_;

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

  // http://crbug.com/90971 - Assists in tracking down use-after-frees on
  // shutdown.
  std::set<const ResourceContext*> active_resource_contexts_;

  typedef std::map<GlobalRequestID,
                   ObserverList<ResourceMessageDelegate>*> DelegateMap;
  DelegateMap delegate_map_;

  scoped_ptr<ResourceScheduler> scheduler_;

  RenderViewHostTracker tracker_;  // Lives on UI thread.

  DISALLOW_COPY_AND_ASSIGN(ResourceDispatcherHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_DISPATCHER_HOST_IMPL_H_
