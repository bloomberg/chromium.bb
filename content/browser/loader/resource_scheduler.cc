// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_scheduler.h"

#include "base/stl_util.h"
#include "content/common/resource_messages.h"
#include "content/browser/loader/resource_message_delegate.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_throttle.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"

namespace content {

// TODO(simonjam): This is arbitrary. Experiment.
static const int kMaxNumNavigationsToTrack = 5;

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
  const net::URLRequest& url_request() const { return *request_; }

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
    net::RequestPriority old_priority = request_->priority();
    request_->set_priority(new_priority);
    if (new_priority > old_priority) {
      Start();
    }
  }

  ClientId client_id_;
  net::URLRequest* request_;
  bool ready_;
  bool deferred_;
  ResourceScheduler* scheduler_;

  DISALLOW_COPY_AND_ASSIGN(ScheduledResourceRequest);
};

ResourceScheduler::ResourceScheduler()
    : client_map_(kMaxNumNavigationsToTrack) {
}

ResourceScheduler::~ResourceScheduler() {
  for (ClientMap::iterator it ALLOW_UNUSED = client_map_.begin();
       it != client_map_.end(); ++it) {
    DCHECK(it->second->pending_requests.empty());
    DCHECK(it->second->in_flight_requests.empty());
  }
  DCHECK(unowned_requests_.empty());
}

scoped_ptr<ResourceThrottle> ResourceScheduler::ScheduleRequest(
    int child_id,
    int route_id,
    net::URLRequest* url_request) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);
  scoped_ptr<ScheduledResourceRequest> request(
      new ScheduledResourceRequest(client_id, url_request, this));

  ClientMap::iterator it = client_map_.Get(client_id);
  if (it == client_map_.end()) {
    // There are several ways this could happen:
    // 1. <a ping> requests don't have a route_id.
    // 2. Most unittests don't send the IPCs needed to register Clients.
    // 3. The tab is closed while a RequestResource IPC is in flight.
    // 4. The tab hasn't navigated recently.
    unowned_requests_.insert(request.get());
    request->Start();
    return request.PassAs<ResourceThrottle>();
  }

  Client* client = it->second;

  bool is_synchronous = (url_request->load_flags() & net::LOAD_IGNORE_LIMITS) ==
      net::LOAD_IGNORE_LIMITS;
  bool is_low_priority =
      url_request->priority() < net::MEDIUM && !is_synchronous;

  if (is_low_priority && !client->in_flight_requests.empty() &&
      !client->has_body) {
    client->pending_requests.push_back(request.get());
  } else {
    StartRequest(request.get(), client);
  }
  return request.PassAs<ResourceThrottle>();
}

void ResourceScheduler::RemoveRequest(ScheduledResourceRequest* request) {
  DCHECK(CalledOnValidThread());
  if (ContainsKey(unowned_requests_, request)) {
    unowned_requests_.erase(request);
    return;
  }

  ClientMap::iterator client_it = client_map_.Get(request->client_id());
  if (client_it == client_map_.end()) {
    return;
  }

  Client* client = client_it->second;
  RequestSet::iterator request_it = client->in_flight_requests.find(request);
  if (request_it == client->in_flight_requests.end()) {
    bool removed = false;
    RequestQueue::iterator queue_it;
    for (queue_it = client->pending_requests.begin();
         queue_it != client->pending_requests.end(); ++queue_it) {
      if (*queue_it == request) {
        client->pending_requests.erase(queue_it);
        removed = true;
        break;
      }
    }
    DCHECK(removed);
    DCHECK(!ContainsKey(client->in_flight_requests, request));
  } else {
    size_t erased = client->in_flight_requests.erase(request);
    DCHECK(erased);
  }

  if (client->in_flight_requests.empty()) {
    // Since the network is now idle, we may as well load some of the low
    // priority requests.
    LoadPendingRequests(client);
  }
}

void ResourceScheduler::OnNavigate(int child_id, int route_id) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);

  ClientMap::iterator it = client_map_.Get(client_id);
  if (it == client_map_.end()) {
    it = client_map_.Put(client_id, new Client(this));
  }

  Client* client = it->second;
  client->has_body = false;
}

void ResourceScheduler::OnWillInsertBody(int child_id, int route_id) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);
  ClientMap::iterator it = client_map_.Get(client_id);
  if (it == client_map_.end()) {
    return;
  }

  Client* client = it->second;
  if (!client->has_body) {
    client->has_body = true;
    LoadPendingRequests(client);
  }
}

void ResourceScheduler::StartRequest(ScheduledResourceRequest* request,
                                     Client* client) {
  client->in_flight_requests.insert(request);
  request->Start();
}

void ResourceScheduler::LoadPendingRequests(Client* client) {
  while (!client->pending_requests.empty()) {
    ScheduledResourceRequest* request = client->pending_requests.front();
    client->pending_requests.erase(client->pending_requests.begin());
    StartRequest(request, client);
  }
}

void ResourceScheduler::RemoveClient(Client* client) {
  LoadPendingRequests(client);
  for (RequestSet::iterator it = client->in_flight_requests.begin();
       it != client->in_flight_requests.end(); ++it) {
    unowned_requests_.insert(*it);
  }
  client->in_flight_requests.clear();
}

ResourceScheduler::ClientId ResourceScheduler::MakeClientId(
    int child_id, int route_id) {
  return (static_cast<ResourceScheduler::ClientId>(child_id) << 32) | route_id;
}

ResourceScheduler::Client::Client(ResourceScheduler* scheduler)
    : has_body(false),
      scheduler_(scheduler) {
}

ResourceScheduler::Client::~Client() {
  scheduler_->RemoveClient(this);
  DCHECK(in_flight_requests.empty());
  DCHECK(pending_requests.empty());
}

}  // namespace content
