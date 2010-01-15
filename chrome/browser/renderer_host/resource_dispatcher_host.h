// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the browser side of the resource dispatcher, it receives requests
// from the child process (i.e. [Renderer, Plugin, Worker]ProcessHost), and
// dispatches them to URLRequests. It then fowards the messages from the
// URLRequests back to the correct process for handling.
//
// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#ifndef CHROME_BROWSER_RENDERER_HOST_RESOURCE_DISPATCHER_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_RESOURCE_DISPATCHER_HOST_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/process.h"
#include "base/timer.h"
#include "chrome/common/child_process_info.h"
#include "chrome/browser/privacy_blacklist/blacklist_interceptor.h"
#include "chrome/browser/renderer_host/resource_queue.h"
#include "ipc/ipc_message.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/resource_type.h"

class BlacklistListener;
class CrossSiteResourceHandler;
class DownloadFileManager;
class DownloadRequestManager;
class LoginHandler;
class PluginService;
class ResourceDispatcherHostRequestInfo;
class ResourceHandler;
class SafeBrowsingService;
class SaveFileManager;
class SocketStreamDispatcherHost;
class SSLClientAuthHandler;
class UserScriptListener;
class URLRequestContext;
class WebKitThread;
struct GlobalRequestID;
struct ViewHostMsg_Resource_Request;
struct ViewMsg_ClosePage_Params;

class ResourceDispatcherHost : public URLRequest::Delegate {
 public:
  // Implemented by the client of ResourceDispatcherHost to receive messages in
  // response to a resource load.  The messages are intended to be forwarded to
  // the ResourceDispatcher in the child process via an IPC channel that the
  // client manages.
  //
  // NOTE: This class unfortunately cannot be named 'Delegate' because that
  // conflicts with the name of ResourceDispatcherHost's base class.
  //
  // If the receiver is unable to send a given message (i.e., if Send returns
  // false), then the ResourceDispatcherHost assumes the receiver has failed,
  // and the given request will be dropped. (This happens, for example, when a
  // renderer crashes and the channel dies).
  class Receiver : public IPC::Message::Sender,
                   public ChildProcessInfo {
   public:
    // Returns the URLRequestContext for the given request.
    // If NULL is returned, the default context for the profile is used.
    virtual URLRequestContext* GetRequestContext(
        uint32 request_id,
        const ViewHostMsg_Resource_Request& request_data) = 0;

   protected:
    explicit Receiver(ChildProcessInfo::ProcessType type, int child_id)
        : ChildProcessInfo(type, child_id) {}
    virtual ~Receiver() {}
  };

  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnRequestStarted(ResourceDispatcherHost* resource_dispatcher,
                                  URLRequest* request) = 0;
    virtual void OnResponseCompleted(
        ResourceDispatcherHost* resource_dispatcher,
        URLRequest* request) = 0;
    virtual void OnReceivedRedirect(ResourceDispatcherHost* resource_dispatcher,
                                    URLRequest* request,
                                    const GURL& new_url) = 0;
  };

  ResourceDispatcherHost();
  ~ResourceDispatcherHost();

  void Initialize();

  // Puts the resource dispatcher host in an inactive state (unable to begin
  // new requests).  Cancels all pending requests.
  void Shutdown();

  // Returns true if the message was a resource message that was processed.
  // If it was, message_was_ok will be false iff the message was corrupt.
  bool OnMessageReceived(const IPC::Message& message,
                         Receiver* receiver,
                         bool* message_was_ok);

  // Initiates a download from the browser process (as opposed to a resource
  // request from the renderer or another child process).
  void BeginDownload(const GURL& url,
                     const GURL& referrer,
                     const FilePath& save_file_path,
                     int process_unique_id,
                     int route_id,
                     URLRequestContext* request_context);

  // Initiates a save file from the browser process (as opposed to a resource
  // request from the renderer or another child process).
  void BeginSaveFile(const GURL& url,
                     const GURL& referrer,
                     int process_unique_id,
                     int route_id,
                     URLRequestContext* request_context);

  // Cancels the given request if it still exists. We ignore cancels from the
  // renderer in the event of a download.
  void CancelRequest(int process_unique_id,
                     int request_id,
                     bool from_renderer);

  // Follows a deferred redirect for the given request.
  // new_first_party_for_cookies, if non-empty, is the new cookie policy URL
  // for the redirected URL.  If the cookie policy URL needs changing, pass
  // true as has_new_first_party_for_cookies and the new cookie policy URL as
  // new_first_party_for_cookies.  Otherwise, pass false as
  // has_new_first_party_for_cookies, and new_first_party_for_cookies will not
  // be used.
  void FollowDeferredRedirect(int process_unique_id,
                              int request_id,
                              bool has_new_first_party_for_cookies,
                              const GURL& new_first_party_for_cookies);

  // Returns true if it's ok to send the data. If there are already too many
  // data messages pending, it pauses the request and returns false. In this
  // case the caller should not send the data.
  bool WillSendData(int process_unique_id, int request_id);

  // Pauses or resumes network activity for a particular request.
  void PauseRequest(int process_unique_id, int request_id, bool pause);

  // Returns the number of pending requests. This is designed for the unittests
  int pending_requests() const {
    return static_cast<int>(pending_requests_.size());
  }

  // Intended for unit-tests only. Returns the memory cost of all the
  // outstanding requests (pending and blocked) for |process_unique_id|.
  int GetOutstandingRequestsMemoryCost(int process_unique_id) const;

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

  DownloadRequestManager* download_request_manager() const {
    return download_request_manager_.get();
  }

  SaveFileManager* save_file_manager() const {
    return save_file_manager_;
  }

  SafeBrowsingService* safe_browsing_service() const {
    return safe_browsing_;
  }

  WebKitThread* webkit_thread() const {
    return webkit_thread_.get();
  }

  // Called when the onunload handler for a cross-site request has finished.
  void OnClosePageACK(const ViewMsg_ClosePage_Params& params);

  // Force cancels any pending requests for the given process.
  void CancelRequestsForProcess(int process_unique_id);

  // Force cancels any pending requests for the given route id.  This method
  // acts like CancelRequestsForProcess when route_id is -1.
  void CancelRequestsForRoute(int process_unique_id, int route_id);

  // URLRequest::Delegate
  virtual void OnReceivedRedirect(URLRequest* request,
                                  const GURL& new_url,
                                  bool* defer_redirect);
  virtual void OnAuthRequired(URLRequest* request,
                              net::AuthChallengeInfo* auth_info);
  virtual void OnCertificateRequested(
      URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info);
  virtual void OnSSLCertificateError(URLRequest* request,
                                     int cert_error,
                                     net::X509Certificate* cert);
  virtual void OnResponseStarted(URLRequest* request);
  virtual void OnReadCompleted(URLRequest* request, int bytes_read);
  void OnResponseCompleted(URLRequest* request);

  // Helper functions to get our extra data out of a request. The given request
  // must have been one we created so that it has the proper extra data pointer.
  static ResourceDispatcherHostRequestInfo* InfoForRequest(URLRequest* request);
  static const ResourceDispatcherHostRequestInfo* InfoForRequest(
      const URLRequest* request);

  // Extracts the render view/process host's identifiers from the given request
  // and places them in the given out params (both required). If there are no
  // such IDs associated with the request (such as non-page-related requests),
  // this function will return false and both out params will be -1.
  static bool RenderViewForRequest(const URLRequest* request,
                                   int* render_process_host_id,
                                   int* render_view_host_id);

  // Adds an observer.  The observer will be called on the IO thread.  To
  // observe resource events on the UI thread, subscribe to the
  // NOTIFY_RESOURCE_* notifications of the notification service.
  void AddObserver(Observer* obs);

  // Removes an observer.
  void RemoveObserver(Observer* obs);

  // Retrieves a URLRequest.  Must be called from the IO thread.
  URLRequest* GetURLRequest(const GlobalRequestID& request_id) const;

  // Notifies our observers that a request has been cancelled.
  void NotifyResponseCompleted(URLRequest* request, int process_unique_id);

  void RemovePendingRequest(int process_unique_id, int request_id);

  // Causes all new requests for the route identified by
  // |process_unique_id| and |route_id| to be blocked (not being
  // started) until ResumeBlockedRequestsForRoute or
  // CancelBlockedRequestsForRoute is called.
  void BlockRequestsForRoute(int process_unique_id, int route_id);

  // Resumes any blocked request for the specified route id.
  void ResumeBlockedRequestsForRoute(int process_unique_id, int route_id);

  // Cancels any blocked request for the specified route id.
  void CancelBlockedRequestsForRoute(int process_unique_id, int route_id);

  // Decrements the pending_data_count for the request and resumes
  // the request if it was paused due to too many pending data
  // messages sent.
  void DataReceivedACK(int process_unique_id, int request_id);

  // Needed for the sync IPC message dispatcher macros.
  bool Send(IPC::Message* message) {
    delete message;
    return false;
  }

 private:
  FRIEND_TEST(ResourceDispatcherHostTest, TestBlockedRequestsProcessDies);
  FRIEND_TEST(ResourceDispatcherHostTest,
              IncrementOutstandingRequestsMemoryCost);
  FRIEND_TEST(ResourceDispatcherHostTest,
              CalculateApproximateMemoryCost);

  class ShutdownTask;

  friend class ShutdownTask;

  // Associates the given info with the given request. The info will then be
  // owned by the request.
  void SetRequestInfo(URLRequest* request,
                      ResourceDispatcherHostRequestInfo* info);

  // A shutdown helper that runs on the IO thread.
  void OnShutdown();

  // Returns true if the request is paused.
  bool PauseRequestIfNeeded(ResourceDispatcherHostRequestInfo* info);

  // Resumes the given request by calling OnResponseStarted or OnReadCompleted.
  void ResumeRequest(const GlobalRequestID& request_id);

  // Internal function to start reading for the first time.
  void StartReading(URLRequest* request);

  // Reads data from the response using our internal buffer as async IO.
  // Returns true if data is available immediately, false otherwise.  If the
  // return value is false, we will receive a OnReadComplete() callback later.
  bool Read(URLRequest* request, int* bytes_read);

  // Internal function to finish an async IO which has completed.  Returns
  // true if there is more data to read (e.g. we haven't read EOF yet and
  // no errors have occurred).
  bool CompleteRead(URLRequest*, int* bytes_read);

  // Internal function to finish handling the ResponseStarted message.  Returns
  // true on success.
  bool CompleteResponseStarted(URLRequest* request);

  // Cancels the given request if it still exists. We ignore cancels from the
  // renderer in the event of a download. If |allow_delete| is true and no IO
  // is pending, the request is removed and deleted.
  void CancelRequest(int process_unique_id,
                     int request_id,
                     bool from_renderer,
                     bool allow_delete);

  // Helper function for regular and download requests.
  void BeginRequestInternal(URLRequest* request);

  // Updates the "cost" of outstanding requests for |process_unique_id|.
  // The "cost" approximates how many bytes are consumed by all the in-memory
  // data structures supporting this request (URLRequest object,
  // HttpNetworkTransaction, etc...).
  // The value of |cost| is added to the running total, and the resulting
  // sum is returned.
  int IncrementOutstandingRequestsMemoryCost(int cost,
                                             int process_unique_id);

  // Estimate how much heap space |request| will consume to run.
  static int CalculateApproximateMemoryCost(URLRequest* request);

  // The list of all requests that we have pending. This list is not really
  // optimized, and assumes that we have relatively few requests pending at once
  // since some operations require brute-force searching of the list.
  //
  // It may be enhanced in the future to provide some kind of prioritization
  // mechanism. We should also consider a hashtable or binary tree if it turns
  // out we have a lot of things here.
  typedef std::map<GlobalRequestID, URLRequest*> PendingRequestList;

  // Deletes the pending request identified by the iterator passed in.
  // This function will invalidate the iterator passed in. Callers should
  // not rely on this iterator being valid on return.
  void RemovePendingRequest(const PendingRequestList::iterator& iter);

  // Notify our observers that we started receiving a response for a request.
  void NotifyResponseStarted(URLRequest* request, int process_unique_id);

  // Notify our observers that a request has been redirected.
  void NotifyReceivedRedirect(URLRequest* request,
                              int process_unique_id,
                              const GURL& new_url);

  // Tries to handle the url with an external protocol. If the request is
  // handled, the function returns true. False otherwise.
  bool HandleExternalProtocol(int request_id,
                              int process_unique_id,
                              int route_id,
                              const GURL& url,
                              ResourceType::Type resource_type,
                              ResourceHandler* handler);

  // Checks all pending requests and updates the load states and upload
  // progress if necessary.
  void UpdateLoadStates();

  // Checks the upload state and sends an update if one is necessary.
  bool MaybeUpdateUploadProgress(ResourceDispatcherHostRequestInfo *info,
                                 URLRequest *request);

  // Resumes or cancels (if |cancel_requests| is true) any blocked requests.
  void ProcessBlockedRequestsForRoute(int process_unique_id,
                                      int route_id,
                                      bool cancel_requests);

  void OnRequestResource(const IPC::Message& msg,
                         int request_id,
                         const ViewHostMsg_Resource_Request& request_data);
  void OnSyncLoad(int request_id,
                  const ViewHostMsg_Resource_Request& request_data,
                  IPC::Message* sync_result);
  void BeginRequest(int request_id,
                    const ViewHostMsg_Resource_Request& request_data,
                    IPC::Message* sync_result,  // only valid for sync
                    int route_id);  // only valid for async
  void OnDataReceivedACK(int request_id);
  void OnUploadProgressACK(int request_id);
  void OnCancelRequest(int request_id);
  void OnFollowRedirect(int request_id,
                        bool has_new_first_party_for_cookies,
                        const GURL& new_first_party_for_cookies);

  // Returns true if the message passed in is a resource related message.
  static bool IsResourceDispatcherHostMessage(const IPC::Message&);

  PendingRequestList pending_requests_;

  // A timer that periodically calls UpdateLoadStates while pending_requests_
  // is not empty.
  base::RepeatingTimer<ResourceDispatcherHost> update_load_states_timer_;

  // Handles the resource requests from the moment we want to start them.
  ResourceQueue resource_queue_;

  // We own the download file writing thread and manager
  scoped_refptr<DownloadFileManager> download_file_manager_;

  // Determines whether a download is allowed.
  scoped_refptr<DownloadRequestManager> download_request_manager_;

  // We own the save file manager.
  scoped_refptr<SaveFileManager> save_file_manager_;

  // Handles requests blocked by privacy blacklists.
  BlacklistInterceptor blacklist_interceptor_;

  // Makes sure that each request is reliably checked against the privacy
  // blacklist.
  scoped_refptr<BlacklistListener> blacklist_listener_;

  scoped_refptr<UserScriptListener> user_script_listener_;

  scoped_refptr<SafeBrowsingService> safe_browsing_;

  scoped_ptr<SocketStreamDispatcherHost> socket_stream_dispatcher_host_;

  // We own the WebKit thread and see to its destruction.
  scoped_ptr<WebKitThread> webkit_thread_;

  // Request ID for browser initiated requests. request_ids generated by
  // child processes are counted up from 0, while browser created requests
  // start at -2 and go down from there. (We need to start at -2 because -1 is
  // used as a special value all over the resource_dispatcher_host for
  // uninitialized variables.) This way, we no longer have the unlikely (but
  // observed in the real world!) event where we have two requests with the same
  // request_id_.
  int request_id_;

  // List of objects observing resource dispatching.
  ObserverList<Observer> observer_list_;

  // For running tasks.
  ScopedRunnableMethodFactory<ResourceDispatcherHost> method_runner_;

  // True if the resource dispatcher host has been shut down.
  bool is_shutdown_;

  typedef std::vector<URLRequest*> BlockedRequestsList;
  typedef std::pair<int, int> ProcessRouteIDs;
  typedef std::map<ProcessRouteIDs, BlockedRequestsList*> BlockedRequestMap;
  BlockedRequestMap blocked_requests_map_;

  // Maps the process_unique_ids to the approximate number of bytes
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

  // Used during IPC message dispatching so that the handlers can get a pointer
  // to the source of the message.
  Receiver* receiver_;

  DISALLOW_COPY_AND_ASSIGN(ResourceDispatcherHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RESOURCE_DISPATCHER_HOST_H_
