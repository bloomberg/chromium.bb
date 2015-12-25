// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
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
// Users should call ScheduleRequest() to notify this ResourceScheduler of a new
// request. The returned ResourceThrottle should be destroyed when the load
// finishes or is canceled, before the net::URLRequest.
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
  // when the load completes or is canceled, before |url_request| is deleted.
  scoped_ptr<ResourceThrottle> ScheduleRequest(int child_id,
                                               int route_id,
                                               bool is_async,
                                               net::URLRequest* url_request);

  // Signals from the UI thread, posted as tasks on the IO thread:

  // Called when a renderer is created.
  void OnClientCreated(int child_id,
                       int route_id,
                       bool is_visible,
                       bool is_audible);

  // Called when a renderer is destroyed.
  void OnClientDeleted(int child_id, int route_id);

  // Called when a renderer stops or restarts loading.
  void OnLoadingStateChanged(int child_id, int route_id, bool is_loaded);

  // Called when a Client is shown or hidden.
  void OnVisibilityChanged(int child_id, int route_id, bool is_visible);

  // Called when a Client starts or stops playing audio.
  void OnAudibilityChanged(int child_id, int route_id, bool is_audible);

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

  bool IsClientVisibleForTesting(int child_id, int route_id);

  // Returns true if at least one client is currently loading.
  bool HasLoadingClients() const;

  // Update the priority for |request|. Modifies request->priority(), and may
  // start the request loading if it wasn't already started.
  void ReprioritizeRequest(net::URLRequest* request,
                           net::RequestPriority new_priority,
                           int intra_priority_value);

 private:
  // Returns true if limiting of outstanding requests is enabled.
  bool limit_outstanding_requests() const {
    return limit_outstanding_requests_;
  }

  // Returns the outstanding request limit.  Only valid if
  // |IsLimitingOutstandingRequests()|.
  size_t outstanding_request_limit() const {
    return outstanding_request_limit_;
  }

  // Returns the priority level above which resources are considered
  // layout-blocking if the html_body has not started.  It is also the threshold
  // below which resources are considered delayable (and for completeness,
  // a request that matches the threshold level is a high-priority but not
  // layout-blocking request).
  net::RequestPriority non_delayable_threshold() const {
    return non_delayable_threshold_;
  }

  // Returns true if all delayable requests should be blocked while at least
  // in_flight_layout_blocking_threshold() layout-blocking requests are
  // in-flight during the layout-blocking phase of loading.
  bool enable_in_flight_non_delayable_threshold() const {
    return enable_in_flight_non_delayable_threshold_;
  }

  // Returns the number of in-flight layout-blocking requests above which
  // all delayable requests should be blocked when
  // enable_layout_blocking_threshold is set.
  size_t in_flight_non_delayable_threshold() const {
    return in_flight_non_delayable_threshold_;
  }

  // Returns the maximum number of delayable requests to allow be in-flight
  // at any point in time while in the layout-blocking phase of loading.
  size_t max_num_delayable_while_layout_blocking() const {
    return max_num_delayable_while_layout_blocking_;
  }

  // Returns the maximum number of delayable requests to all be in-flight at
  // any point in time (across all hosts).
  size_t max_num_delayable_requests() const {
    return max_num_delayable_requests_;
  }

  enum ClientState {
    // Observable client.
    ACTIVE,
    // Non-observable client.
    BACKGROUND,
    // No client found.
    UNKNOWN,
  };

  class RequestQueue;
  class ScheduledResourceRequest;
  struct RequestPriorityParams;
  struct ScheduledResourceSorter {
    bool operator()(const ScheduledResourceRequest* a,
                    const ScheduledResourceRequest* b) const;
  };
  class Client;

  typedef int64_t ClientId;
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

  // Returns UNKNOWN if the corresponding client is not found, else returns
  // whether the client is ACTIVE (user-observable) or BACKGROUND.
  ClientState GetClientState(ClientId client_id) const;

  // Returns the client ID for the given |child_id| and |route_id| combo.
  ClientId MakeClientId(int child_id, int route_id);

  // Returns the client for the given |child_id| and |route_id| combo.
  Client* GetClient(int child_id, int route_id);

  bool should_coalesce_;
  bool should_throttle_;
  ClientMap client_map_;
  size_t active_clients_loading_;
  size_t coalesced_clients_;
  bool limit_outstanding_requests_;
  size_t outstanding_request_limit_;
  net::RequestPriority non_delayable_threshold_;
  bool enable_in_flight_non_delayable_threshold_;
  size_t in_flight_non_delayable_threshold_;
  size_t max_num_delayable_while_layout_blocking_;
  size_t max_num_delayable_requests_;
  // This is a repeating timer to initiate requests on COALESCED Clients.
  scoped_ptr<base::Timer> coalescing_timer_;
  RequestSet unowned_requests_;

  DISALLOW_COPY_AND_ASSIGN(ResourceScheduler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_
