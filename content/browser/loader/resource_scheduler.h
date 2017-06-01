// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "net/base/priority_queue.h"
#include "net/base/request_priority.h"

namespace net {
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
class CONTENT_EXPORT ResourceScheduler {
 public:
  ResourceScheduler();
  ~ResourceScheduler();

  // Requests that this ResourceScheduler schedule, and eventually loads, the
  // specified |url_request|. Caller should delete the returned ResourceThrottle
  // when the load completes or is canceled, before |url_request| is deleted.
  std::unique_ptr<ResourceThrottle> ScheduleRequest(
      int child_id,
      int route_id,
      bool is_async,
      net::URLRequest* url_request);

  // Signals from the UI thread, posted as tasks on the IO thread:

  // Called when a renderer is created.
  void OnClientCreated(int child_id, int route_id);

  // Called when a renderer is destroyed.
  void OnClientDeleted(int child_id, int route_id);

  // Called when a renderer stops or restarts loading.
  void OnLoadingStateChanged(int child_id, int route_id, bool is_loaded);

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

  // Returns true if at least one client is currently loading.
  bool HasLoadingClients() const;

  // Updates the priority for |request|. Modifies request->priority(), and may
  // start the request loading if it wasn't already started.
  // If the scheduler does not know about the request, |new_priority| is set but
  // |intra_priority_value| is ignored.
  void ReprioritizeRequest(net::URLRequest* request,
                           net::RequestPriority new_priority,
                           int intra_priority_value);
  // Same as above, but keeps the existing intra priority value.
  void ReprioritizeRequest(net::URLRequest* request,
                           net::RequestPriority new_priority);

 private:
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

  // Returns the client ID for the given |child_id| and |route_id| combo.
  ClientId MakeClientId(int child_id, int route_id);

  // Returns the client for the given |child_id| and |route_id| combo.
  Client* GetClient(int child_id, int route_id);

  ClientMap client_map_;
  RequestSet unowned_requests_;

  // True if requests to servers that support priorities (e.g., H2/QUIC) can
  // be delayed.
  bool priority_requests_delayable_;

  // True if the scheduler should yield between several successive calls to
  // start resource requests.
  bool yielding_scheduler_enabled_;
  int max_requests_before_yielding_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ResourceScheduler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_
