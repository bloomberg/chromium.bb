// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "net/base/priority_queue.h"
#include "net/base/request_priority.h"

namespace net {
class HostPortPair;
class URLRequest;
}

namespace content {
class ResourceThrottle;

// There is one ResourceScheduler. All renderer-initiated HTTP requests are
// expected to pass through it.
//
// There are two types of input to the scheduler:
// 1. Requests to start, cancel, or finish fetching a resource.
// 2. Notifications for renderer events, such as new tabs, navigation and
//    painting.
//
// These input come from different threads, so they may not be in sync. The UI
// thread is considered the authority on renderer lifetime, which means some
// IPCs may be meaningless if they arrive after the UI thread signals a renderer
// has been deleted.
//
// The ResourceScheduler tracks many Clients, which should correlate with tabs.
// A client is uniquely identified by its child_id and route_id.
//
// Each Client may have many Requests in flight. Requests are uniquely
// identified within a Client by its ScheduledResourceRequest.
//
// Users should call ScheduleRequest() to notify this ResourceScheduler of a
// new request. The returned ResourceThrottle should be destroyed when the load
// finishes or is canceled.
//
// The scheduler may defer issuing the request via the ResourceThrottle
// interface or it may alter the request's priority by calling set_priority() on
// the URLRequest.
class CONTENT_EXPORT ResourceScheduler : public base::NonThreadSafe {
 public:
  enum ClientThrottleState {
    // TODO(aiolos): Add logic to ShouldStartRequest for PAUSED Clients to only
    // issue synchronous requests.
    // TODO(aiolos): Add max number of THROTTLED Clients, and logic to set
    // subsquent Clients to PAUSED instead. Also add logic to unpause a Client
    // when a background Client becomes COALESCED (ie, finishes loading.)
    // TODO(aiolos): Add tests for the above mentioned logic.

    // Currently being deleted client.
    // This state currently follows the same logic for loading requests as
    // UNTHROTTLED/ACTIVE_AND_LOADING Clients. See above TODO's.
    PAUSED,
    // Loaded background client, all observable clients loaded.
    COALESCED,
    // Background client, an observable client is loading.
    THROTTLED,
    // Observable (active) loaded client or
    // Loading background client, all observable clients loaded.
    // Note that clients which would be COALESCED are UNTHROTTLED until
    // coalescing is turned on.
    UNTHROTTLED,
    // Observable (active) loading client.
    ACTIVE_AND_LOADING,
  };

  enum RequestClassification {
    NORMAL_REQUEST,
    // Low priority in-flight requests
    IN_FLIGHT_DELAYABLE_REQUEST,
    // High-priority requests received before the renderer has a <body>
    LAYOUT_BLOCKING_REQUEST,
  };

  ResourceScheduler();
  ~ResourceScheduler();

  // Use a mock timer when testing.
  void set_timer_for_testing(scoped_ptr<base::Timer> timer) {
    coalescing_timer_.reset(timer.release());
  }

  // TODO(aiolos): Remove when throttling and coalescing have landed
  void SetThrottleOptionsForTesting(bool should_throttle, bool should_coalesce);

  bool should_coalesce() const { return should_coalesce_; }
  bool should_throttle() const { return should_throttle_; }

  ClientThrottleState GetClientStateForTesting(int child_id, int route_id);

  // Requests that this ResourceScheduler schedule, and eventually loads, the
  // specified |url_request|. Caller should delete the returned ResourceThrottle
  // when the load completes or is canceled.
  scoped_ptr<ResourceThrottle> ScheduleRequest(
      int child_id, int route_id, net::URLRequest* url_request);

  // Signals from the UI thread, posted as tasks on the IO thread:

  // Called when a renderer is created.
  void OnClientCreated(int child_id, int route_id);

  // Called when a renderer is destroyed.
  void OnClientDeleted(int child_id, int route_id);

  // Signals from IPC messages directly from the renderers:

  // Called when a client navigates to a new main document.
  void OnNavigate(int child_id, int route_id);

  // Called when the client has parsed the <body> element. This is a signal that
  // resource loads won't interfere with first paint.
  void OnWillInsertBody(int child_id, int route_id);

  // Signals from the IO thread:

  // Called when we received a response to a http request that was served
  // from a proxy using SPDY.
  void OnReceivedSpdyProxiedHttpResponse(int child_id, int route_id);

  // Client functions:

  // Called to check if all user observable tabs have completed loading.
  bool active_clients_loaded() const { return active_clients_loading_ == 0; }

  // Called when a Client starts or stops playing audio.
  void OnAudibilityChanged(int child_id, int route_id, bool is_audible);

  // Called when a Client is shown or hidden.
  void OnVisibilityChanged(int child_id, int route_id, bool is_visible);

  void OnLoadingStateChanged(int child_id, int route_id, bool is_loaded);

 private:
  class RequestQueue;
  class ScheduledResourceRequest;
  struct RequestPriorityParams;
  struct ScheduledResourceSorter {
    bool operator()(const ScheduledResourceRequest* a,
                    const ScheduledResourceRequest* b) const;
  };
  class Client;

  typedef int64 ClientId;
  typedef std::map<ClientId, Client*> ClientMap;
  typedef std::set<ScheduledResourceRequest*> RequestSet;

  // Called when a ScheduledResourceRequest is destroyed.
  void RemoveRequest(ScheduledResourceRequest* request);

  // These calls may update the ThrottleState of all clients, and have the
  // potential to be re-entrant.
  // Called when a Client newly becomes active loading.
  void IncrementActiveClientsLoading();
  // Called when an active and loading Client either completes loading or
  // becomes inactive.
  void DecrementActiveClientsLoading();

  void OnLoadingActiveClientsStateChangedForAllClients();

  size_t CountActiveClientsLoading() const;

  // Called when a Client becomes coalesced.
  void IncrementCoalescedClients();
  // Called when a client stops being coalesced.
  void DecrementCoalescedClients();

  void LoadCoalescedRequests();

  size_t CountCoalescedClients() const;

  // Update the queue position for |request|, possibly causing it to start
  // loading.
  //
  // Queues are maintained for each priority level. When |request| is
  // reprioritized, it will move to the end of the queue for that priority
  // level.
  void ReprioritizeRequest(ScheduledResourceRequest* request,
                           net::RequestPriority new_priority,
                           int intra_priority_value);

  // Returns the client ID for the given |child_id| and |route_id| combo.
  ClientId MakeClientId(int child_id, int route_id);

  // Returns the client for the given |child_id| and |route_id| combo.
  Client* GetClient(int child_id, int route_id);

  bool should_coalesce_;
  bool should_throttle_;
  ClientMap client_map_;
  size_t active_clients_loading_;
  size_t coalesced_clients_;
  // This is a repeating timer to initiate requests on COALESCED Clients.
  scoped_ptr<base::Timer> coalescing_timer_;
  RequestSet unowned_requests_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_
