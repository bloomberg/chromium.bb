// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/mru_cache.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"

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
// 2. Notifications for renderer events, such as navigation and painting.
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
//
// The scheduler only tracks the most recently used Clients. If a tab hasn't
// navigated or fetched a resource for some time, its state may be forgotten
// until its next navigation. In such situations, no request throttling occurs.
class CONTENT_EXPORT ResourceScheduler : public base::NonThreadSafe {
 public:
  ResourceScheduler();
  ~ResourceScheduler();

  // Requests that this ResourceScheduler schedule, and eventually loads, the
  // specified |url_request|. Caller should delete the returned ResourceThrottle
  // when the load completes or is canceled.
  scoped_ptr<ResourceThrottle> ScheduleRequest(
      int child_id, int route_id, net::URLRequest* url_request);

  // Called when a client navigates to a new main document.
  void OnNavigate(int child_id, int route_id);

  // Called when the client has parsed the <body> element. This is a signal that
  // resource loads won't interfere with first paint.
  void OnWillInsertBody(int child_id, int route_id);

 private:
  class ScheduledResourceRequest;
  friend class ScheduledResourceRequest;
  struct Client;

  typedef int64 ClientId;
  typedef base::OwningMRUCache<ClientId, Client*> ClientMap;
  typedef std::vector<ScheduledResourceRequest*> RequestQueue;
  typedef std::set<ScheduledResourceRequest*> RequestSet;

  struct Client {
    Client(ResourceScheduler* scheduler);
    ~Client();

    bool has_body;
    RequestQueue pending_requests;
    RequestSet in_flight_requests;

   private:
    ResourceScheduler* scheduler_;
  };

  // Called when a ScheduledResourceRequest is destroyed.
  void RemoveRequest(ScheduledResourceRequest* request);

  // Unthrottles the |request| and adds it to |client|.
  void StartRequest(ScheduledResourceRequest* request, Client* client);

  // Calls StartRequest on all pending requests for |client|.
  void LoadPendingRequests(Client* client);

  // Called when a Client is evicted from the MRUCache.
  void RemoveClient(Client* client);

  // Returns the client ID for the given |child_id| and |route_id| combo.
  ClientId MakeClientId(int child_id, int route_id);

  ClientMap client_map_;
  RequestSet unowned_requests_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_
