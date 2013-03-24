// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_scheduler.h"

#include "base/stl_util.h"
#include "content/common/resource_messages.h"
#include "content/browser/loader/resource_message_delegate.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/resource_throttle.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"

namespace content {

static const size_t kMaxNumDelayableRequestsPerClient = 10;

// A thin wrapper around net::PriorityQueue that deals with
// ScheduledResourceRequests instead of PriorityQueue::Pointers.
class ResourceScheduler::RequestQueue {
 public:
  RequestQueue() : queue_(net::NUM_PRIORITIES) {}
  ~RequestQueue() {}

  // Adds |request| to the queue with given |priority|.
  void Insert(ScheduledResourceRequest* request,
              net::RequestPriority priority) {
    DCHECK(!ContainsKey(pointers_, request));
    NetQueue::Pointer pointer = queue_.Insert(request, priority);
    pointers_[request] = pointer;
  }

  // Removes |request| from the queue.
  void Erase(ScheduledResourceRequest* request) {
    PointerMap::iterator it = pointers_.find(request);
    DCHECK(it != pointers_.end());
    queue_.Erase(it->second);
    pointers_.erase(it);
  }

  // Returns the highest priority request that's queued, or NULL if none are.
  ScheduledResourceRequest* FirstMax() {
    return queue_.FirstMax().value();
  }

  // Returns true if |request| is queued.
  bool IsQueued(ScheduledResourceRequest* request) const {
    return ContainsKey(pointers_, request);
  }

  // Returns true if no requests are queued.
  bool IsEmpty() const { return queue_.size() == 0; }

 private:
  typedef net::PriorityQueue<ScheduledResourceRequest*> NetQueue;
  typedef std::map<ScheduledResourceRequest*, NetQueue::Pointer> PointerMap;

  NetQueue queue_;
  PointerMap pointers_;
};

// This is the handle we return to the ResourceDispatcherHostImpl so it can
// interact with the request.
class ResourceScheduler::ScheduledResourceRequest
    : public ResourceMessageDelegate,
      public ResourceThrottle {
 public:
  ScheduledResourceRequest(const ClientId& client_id,
                           net::URLRequest* request,
                           ResourceScheduler* scheduler)
      : ResourceMessageDelegate(request),
        client_id_(client_id),
        request_(request),
        ready_(false),
        deferred_(false),
        scheduler_(scheduler) {
  }

  virtual ~ScheduledResourceRequest() {
    scheduler_->RemoveRequest(this);
  }

  void Start() {
    ready_ = true;
    if (deferred_ && request_->status().is_success()) {
      deferred_ = false;
      controller()->Resume();
    }
  }

  const ClientId& client_id() const { return client_id_; }
  net::URLRequest* url_request() { return request_; }
  const net::URLRequest* url_request() const { return request_; }

 private:
  // ResourceMessageDelegate interface:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP_EX(ScheduledResourceRequest, message, *message_was_ok)
      IPC_MESSAGE_HANDLER(ResourceHostMsg_DidChangePriority, DidChangePriority)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP_EX()
    return handled;
  }

  // ResourceThrottle interface:
  virtual void WillStartRequest(bool* defer) OVERRIDE {
    deferred_ = *defer = !ready_;
  }

  void DidChangePriority(int request_id, net::RequestPriority new_priority) {
    scheduler_->ReprioritizeRequest(this, new_priority);
  }

  ClientId client_id_;
  net::URLRequest* request_;
  bool ready_;
  bool deferred_;
  ResourceScheduler* scheduler_;

  DISALLOW_COPY_AND_ASSIGN(ScheduledResourceRequest);
};

// Each client represents a tab.
struct ResourceScheduler::Client {
  Client() : has_body(false) {}
  ~Client() {}

  bool has_body;
  RequestQueue pending_requests;
  RequestSet in_flight_requests;
};

ResourceScheduler::ResourceScheduler() {
}

ResourceScheduler::~ResourceScheduler() {
  DCHECK(unowned_requests_.empty());
  DCHECK(client_map_.empty());
}

scoped_ptr<ResourceThrottle> ResourceScheduler::ScheduleRequest(
    int child_id,
    int route_id,
    net::URLRequest* url_request) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);
  scoped_ptr<ScheduledResourceRequest> request(
      new ScheduledResourceRequest(client_id, url_request, this));

  ClientMap::iterator it = client_map_.find(client_id);
  if (it == client_map_.end()) {
    // There are several ways this could happen:
    // 1. <a ping> requests don't have a route_id.
    // 2. Most unittests don't send the IPCs needed to register Clients.
    // 3. The tab is closed while a RequestResource IPC is in flight.
    unowned_requests_.insert(request.get());
    request->Start();
    return request.PassAs<ResourceThrottle>();
  }

  Client* client = it->second;
  if (ShouldStartRequest(request.get(), client)) {
    StartRequest(request.get(), client);
  } else {
    client->pending_requests.Insert(request.get(), url_request->priority());
  }
  return request.PassAs<ResourceThrottle>();
}

void ResourceScheduler::RemoveRequest(ScheduledResourceRequest* request) {
  DCHECK(CalledOnValidThread());
  if (ContainsKey(unowned_requests_, request)) {
    unowned_requests_.erase(request);
    return;
  }

  ClientMap::iterator client_it = client_map_.find(request->client_id());
  if (client_it == client_map_.end()) {
    return;
  }

  Client* client = client_it->second;

  if (client->pending_requests.IsQueued(request)) {
    client->pending_requests.Erase(request);
    DCHECK(!ContainsKey(client->in_flight_requests, request));
  } else {
    size_t erased = client->in_flight_requests.erase(request);
    DCHECK(erased);

    // Removing this request may have freed up another to load.
    LoadAnyStartablePendingRequests(client);
  }
}

void ResourceScheduler::OnClientCreated(int child_id, int route_id) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);
  DCHECK(!ContainsKey(client_map_, client_id));

  client_map_[client_id] = new Client;
}

void ResourceScheduler::OnClientDeleted(int child_id, int route_id) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);
  DCHECK(ContainsKey(client_map_, client_id));
  ClientMap::iterator it = client_map_.find(client_id);
  Client* client = it->second;

  // FYI, ResourceDispatcherHost cancels all of the requests after this function
  // is called. It should end up canceling all of the requests except for a
  // cross-renderer navigation.
  for (RequestSet::iterator it = client->in_flight_requests.begin();
       it != client->in_flight_requests.end(); ++it) {
    unowned_requests_.insert(*it);
  }
  client->in_flight_requests.clear();

  delete client;
  client_map_.erase(it);
}

void ResourceScheduler::OnNavigate(int child_id, int route_id) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);

  ClientMap::iterator it = client_map_.find(client_id);
  if (it == client_map_.end()) {
    // The client was likely deleted shortly before we received this IPC.
    return;
  }

  Client* client = it->second;
  client->has_body = false;
}

void ResourceScheduler::OnWillInsertBody(int child_id, int route_id) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);

  ClientMap::iterator it = client_map_.find(client_id);
  if (it == client_map_.end()) {
    // The client was likely deleted shortly before we received this IPC.
    return;
  }

  Client* client = it->second;
  client->has_body = false;
  if (!client->has_body) {
    client->has_body = true;
    LoadAnyStartablePendingRequests(client);
  }
}

void ResourceScheduler::StartRequest(ScheduledResourceRequest* request,
                                     Client* client) {
  client->in_flight_requests.insert(request);
  request->Start();
}

void ResourceScheduler::ReprioritizeRequest(ScheduledResourceRequest* request,
                                            net::RequestPriority new_priority) {
  net::RequestPriority old_priority = request->url_request()->priority();
  DCHECK_NE(new_priority, old_priority);
  request->url_request()->SetPriority(new_priority);
  ClientMap::iterator client_it = client_map_.find(request->client_id());
  if (client_it == client_map_.end()) {
    // The client was likely deleted shortly before we received this IPC.
    return;
  }

  Client *client = client_it->second;
  if (!client->pending_requests.IsQueued(request)) {
    DCHECK(ContainsKey(client->in_flight_requests, request));
    // Request has already started.
    return;
  }

  client->pending_requests.Erase(request);
  client->pending_requests.Insert(request, request->url_request()->priority());

  if (new_priority > old_priority) {
    // Check if this request is now able to load at its new priority.
    LoadAnyStartablePendingRequests(client);
  }
}

void ResourceScheduler::LoadAnyStartablePendingRequests(Client* client) {
  while (!client->pending_requests.IsEmpty()) {
    ScheduledResourceRequest* request = client->pending_requests.FirstMax();
    if (ShouldStartRequest(request, client)) {
      client->pending_requests.Erase(request);
      StartRequest(request, client);
    } else {
      break;
    }
  }
}

size_t ResourceScheduler::GetNumDelayableRequestsInFlight(
    Client* client) const {
  size_t count = 0;
  for (RequestSet::iterator it = client->in_flight_requests.begin();
       it != client->in_flight_requests.end(); ++it) {
    if ((*it)->url_request()->priority() < net::LOW) {
      ++count;
    }
  }
  return count;
}

// ShouldStartRequest is the main scheduling algorithm.
//
// Requests are categorized into two categories:
//
// 1. Immediately issued requests, which are:
//
//   * Higher priority requests (>= net::LOW).
//   * Synchronous requests.
//
// 2. The remainder are delayable requests, which follow these rules:
//
//   * If no high priority requests are in flight, start loading low priority
//     requests.
//   * Once the renderer has a <body>, start loading delayable requests.
//   * Never exceed 10 delayable requests in flight per client.
bool ResourceScheduler::ShouldStartRequest(ScheduledResourceRequest* request,
                                           Client* client) const {
  if (request->url_request()->priority() >= net::LOW ||
      !ResourceRequestInfo::ForRequest(request->url_request())->IsAsync()) {
    return true;
  }

  size_t num_delayable_requests_in_flight =
      GetNumDelayableRequestsInFlight(client);
  if (num_delayable_requests_in_flight >= kMaxNumDelayableRequestsPerClient) {
    return false;
  }

  bool have_immediate_requests_in_flight =
      client->in_flight_requests.size() > num_delayable_requests_in_flight;
  if (have_immediate_requests_in_flight && !client->has_body) {
    return false;
  }

  return true;
}

ResourceScheduler::ClientId ResourceScheduler::MakeClientId(
    int child_id, int route_id) {
  return (static_cast<ResourceScheduler::ClientId>(child_id) << 32) | route_id;
}

}  // namespace content
