// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "content/browser/loader/resource_scheduler.h"

#include "base/stl_util.h"
#include "content/common/resource_messages.h"
#include "content/browser/loader/resource_message_delegate.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/resource_throttle.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/http/http_server_properties.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace content {

static const size_t kCoalescedTimerPeriod = 5000;
static const size_t kMaxNumDelayableRequestsPerClient = 10;
static const size_t kMaxNumDelayableRequestsPerHost = 6;
static const size_t kMaxNumThrottledRequestsPerClient = 1;

struct ResourceScheduler::RequestPriorityParams {
  RequestPriorityParams()
    : priority(net::DEFAULT_PRIORITY),
      intra_priority(0) {
  }

  RequestPriorityParams(net::RequestPriority priority, int intra_priority)
    : priority(priority),
      intra_priority(intra_priority) {
  }

  bool operator==(const RequestPriorityParams& other) const {
    return (priority == other.priority) &&
        (intra_priority == other.intra_priority);
  }

  bool operator!=(const RequestPriorityParams& other) const {
    return !(*this == other);
  }

  bool GreaterThan(const RequestPriorityParams& other) const {
    if (priority != other.priority)
      return priority > other.priority;
    return intra_priority > other.intra_priority;
  }

  net::RequestPriority priority;
  int intra_priority;
};

class ResourceScheduler::RequestQueue {
 public:
  typedef std::multiset<ScheduledResourceRequest*, ScheduledResourceSorter>
      NetQueue;

  RequestQueue() : fifo_ordering_ids_(0) {}
  ~RequestQueue() {}

  // Adds |request| to the queue with given |priority|.
  void Insert(ScheduledResourceRequest* request);

  // Removes |request| from the queue.
  void Erase(ScheduledResourceRequest* request) {
    PointerMap::iterator it = pointers_.find(request);
    DCHECK(it != pointers_.end());
    if (it == pointers_.end())
      return;
    queue_.erase(it->second);
    pointers_.erase(it);
  }

  NetQueue::iterator GetNextHighestIterator() {
    return queue_.begin();
  }

  NetQueue::iterator End() {
    return queue_.end();
  }

  // Returns true if |request| is queued.
  bool IsQueued(ScheduledResourceRequest* request) const {
    return ContainsKey(pointers_, request);
  }

  // Returns true if no requests are queued.
  bool IsEmpty() const { return queue_.size() == 0; }

 private:
  typedef std::map<ScheduledResourceRequest*, NetQueue::iterator> PointerMap;

  uint32 MakeFifoOrderingId() {
    fifo_ordering_ids_ += 1;
    return fifo_ordering_ids_;
  }

  // Used to create an ordering ID for scheduled resources so that resources
  // with same priority/intra_priority stay in fifo order.
  uint32 fifo_ordering_ids_;

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
                           ResourceScheduler* scheduler,
                           const RequestPriorityParams& priority)
      : ResourceMessageDelegate(request),
        client_id_(client_id),
        request_(request),
        ready_(false),
        deferred_(false),
        classification_(NORMAL_REQUEST),
        scheduler_(scheduler),
        priority_(priority),
        fifo_ordering_(0) {
    TRACE_EVENT_ASYNC_BEGIN1("net", "URLRequest", request_,
                             "url", request->url().spec());
  }

  virtual ~ScheduledResourceRequest() {
    scheduler_->RemoveRequest(this);
  }

  void Start() {
    TRACE_EVENT_ASYNC_STEP_PAST0("net", "URLRequest", request_, "Queued");
    ready_ = true;
    if (deferred_ && request_->status().is_success()) {
      deferred_ = false;
      controller()->Resume();
    }
  }

  void set_request_priority_params(const RequestPriorityParams& priority) {
    priority_ = priority;
  }
  const RequestPriorityParams& get_request_priority_params() const {
    return priority_;
  }
  const ClientId& client_id() const { return client_id_; }
  net::URLRequest* url_request() { return request_; }
  const net::URLRequest* url_request() const { return request_; }
  uint32 fifo_ordering() const { return fifo_ordering_; }
  void set_fifo_ordering(uint32 fifo_ordering) {
    fifo_ordering_ = fifo_ordering;
  }
  RequestClassification classification() const {
    return classification_;
  }
  void set_classification(RequestClassification classification) {
    classification_ = classification;
  }

 private:
  // ResourceMessageDelegate interface:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(ScheduledResourceRequest, message)
      IPC_MESSAGE_HANDLER(ResourceHostMsg_DidChangePriority, DidChangePriority)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  // ResourceThrottle interface:
  virtual void WillStartRequest(bool* defer) OVERRIDE {
    deferred_ = *defer = !ready_;
  }

  virtual const char* GetNameForLogging() const OVERRIDE {
    return "ResourceScheduler";
  }

  void DidChangePriority(int request_id, net::RequestPriority new_priority,
                         int intra_priority_value) {
    scheduler_->ReprioritizeRequest(this, new_priority, intra_priority_value);
  }

  ClientId client_id_;
  net::URLRequest* request_;
  bool ready_;
  bool deferred_;
  RequestClassification classification_;
  ResourceScheduler* scheduler_;
  RequestPriorityParams priority_;
  uint32 fifo_ordering_;

  DISALLOW_COPY_AND_ASSIGN(ScheduledResourceRequest);
};

bool ResourceScheduler::ScheduledResourceSorter::operator()(
    const ScheduledResourceRequest* a,
    const ScheduledResourceRequest* b) const {
  // Want the set to be ordered first by decreasing priority, then by
  // decreasing intra_priority.
  // ie. with (priority, intra_priority)
  // [(1, 0), (1, 0), (0, 100), (0, 0)]
  if (a->get_request_priority_params() != b->get_request_priority_params())
    return a->get_request_priority_params().GreaterThan(
        b->get_request_priority_params());

  // If priority/intra_priority is the same, fall back to fifo ordering.
  // std::multiset doesn't guarantee this until c++11.
  return a->fifo_ordering() < b->fifo_ordering();
}

void ResourceScheduler::RequestQueue::Insert(
    ScheduledResourceRequest* request) {
  DCHECK(!ContainsKey(pointers_, request));
  request->set_fifo_ordering(MakeFifoOrderingId());
  pointers_[request] = queue_.insert(request);
}

// Each client represents a tab.
class ResourceScheduler::Client {
 public:
  explicit Client(ResourceScheduler* scheduler)
      : is_audible_(false),
        is_visible_(false),
        is_loaded_(false),
        is_paused_(false),
        has_body_(false),
        using_spdy_proxy_(false),
        in_flight_delayable_count_(0),
        total_layout_blocking_count_(0),
        throttle_state_(ResourceScheduler::THROTTLED) {
    scheduler_ = scheduler;
  }

  ~Client() {
    // Update to default state and pause to ensure the scheduler has a
    // correct count of relevant types of clients.
    is_visible_ = false;
    is_audible_ = false;
    is_paused_ = true;
    UpdateThrottleState();
  }

  void ScheduleRequest(
      net::URLRequest* url_request,
      ScheduledResourceRequest* request) {
    if (ShouldStartRequest(request) == START_REQUEST)
      StartRequest(request);
    else
      pending_requests_.Insert(request);
    SetRequestClassification(request, ClassifyRequest(request));
  }

  void RemoveRequest(ScheduledResourceRequest* request) {
    if (pending_requests_.IsQueued(request)) {
      pending_requests_.Erase(request);
      DCHECK(!ContainsKey(in_flight_requests_, request));
    } else {
      EraseInFlightRequest(request);

      // Removing this request may have freed up another to load.
      LoadAnyStartablePendingRequests();
    }
  }

  RequestSet RemoveAllRequests() {
    RequestSet unowned_requests;
    for (RequestSet::iterator it = in_flight_requests_.begin();
         it != in_flight_requests_.end(); ++it) {
      unowned_requests.insert(*it);
      (*it)->set_classification(NORMAL_REQUEST);
    }
    ClearInFlightRequests();
    return unowned_requests;
  }

  bool is_active() const { return is_visible_ || is_audible_; }

  bool is_loaded() const { return is_loaded_; }

  void OnAudibilityChanged(bool is_audible) {
    if (is_audible == is_audible_) {
      return;
    }
    is_audible_ = is_audible;
    UpdateThrottleState();
  }

  void OnVisibilityChanged(bool is_visible) {
    if (is_visible == is_visible_) {
      return;
    }
    is_visible_ = is_visible;
    UpdateThrottleState();
  }

  void OnLoadingStateChanged(bool is_loaded) {
    if (is_loaded == is_loaded_) {
      return;
    }
    is_loaded_ = is_loaded;
    UpdateThrottleState();
  }

  void SetPaused() {
    is_paused_ = true;
    UpdateThrottleState();
  }

  void UpdateThrottleState() {
    ClientThrottleState old_throttle_state = throttle_state_;
    if (is_active() && !is_loaded_) {
      SetThrottleState(ACTIVE_AND_LOADING);
    } else if (is_active()) {
      SetThrottleState(UNTHROTTLED);
    } else if (is_paused_) {
      SetThrottleState(PAUSED);
    } else if (!scheduler_->active_clients_loaded()) {
      SetThrottleState(THROTTLED);
    } else if (is_loaded_ && scheduler_->should_coalesce()) {
      SetThrottleState(COALESCED);
    } else if (!is_active()) {
      SetThrottleState(UNTHROTTLED);
    }

    if (throttle_state_ == old_throttle_state) {
      return;
    }
    if (throttle_state_ == ACTIVE_AND_LOADING) {
      scheduler_->IncrementActiveClientsLoading();
    } else if (old_throttle_state == ACTIVE_AND_LOADING) {
      scheduler_->DecrementActiveClientsLoading();
    }
    if (throttle_state_ == COALESCED) {
      scheduler_->IncrementCoalescedClients();
    } else if (old_throttle_state == COALESCED) {
      scheduler_->DecrementCoalescedClients();
    }
  }

  void OnNavigate() {
    has_body_ = false;
    is_loaded_ = false;
  }

  void OnWillInsertBody() {
    has_body_ = true;
    LoadAnyStartablePendingRequests();
  }

  void OnReceivedSpdyProxiedHttpResponse() {
    if (!using_spdy_proxy_) {
      using_spdy_proxy_ = true;
      LoadAnyStartablePendingRequests();
    }
  }

  void ReprioritizeRequest(ScheduledResourceRequest* request,
                           RequestPriorityParams old_priority_params,
                           RequestPriorityParams new_priority_params) {
    request->url_request()->SetPriority(new_priority_params.priority);
    request->set_request_priority_params(new_priority_params);
    if (!pending_requests_.IsQueued(request)) {
      DCHECK(ContainsKey(in_flight_requests_, request));
      // The priority and SPDY support may have changed, so update the
      // delayable count.
      SetRequestClassification(request, ClassifyRequest(request));
      // Request has already started.
      return;
    }

    pending_requests_.Erase(request);
    pending_requests_.Insert(request);

    if (new_priority_params.priority > old_priority_params.priority) {
      // Check if this request is now able to load at its new priority.
      LoadAnyStartablePendingRequests();
    }
  }

  // Called on Client creation, when a Client changes user observability,
  // possibly when all observable Clients have finished loading, and
  // possibly when this Client has finished loading.
  // State changes:
  // Client became observable.
  //   any state -> UNTHROTTLED
  // Client is unobservable, but all observable clients finished loading.
  //   THROTTLED -> UNTHROTTLED
  // Non-observable client finished loading.
  //   THROTTLED || UNTHROTTLED -> COALESCED
  // Non-observable client, an observable client starts loading.
  //   COALESCED -> THROTTLED
  // A COALESCED client will transition into UNTHROTTLED when the network is
  // woken up by a heartbeat and then transition back into COALESCED.
  void SetThrottleState(ResourceScheduler::ClientThrottleState throttle_state) {
    if (throttle_state == throttle_state_) {
      return;
    }
    throttle_state_ = throttle_state;
    if (throttle_state_ != PAUSED) {
      is_paused_ = false;
    }
    LoadAnyStartablePendingRequests();
    // TODO(aiolos): Stop any started but not inflght requests when
    // switching to stricter throttle state?
  }

  ResourceScheduler::ClientThrottleState throttle_state() const {
    return throttle_state_;
  }

  void LoadCoalescedRequests() {
    if (throttle_state_ != COALESCED) {
      return;
    }
    if (scheduler_->active_clients_loaded()) {
      SetThrottleState(UNTHROTTLED);
    } else {
      SetThrottleState(THROTTLED);
    }
    LoadAnyStartablePendingRequests();
    SetThrottleState(COALESCED);
  }

 private:
  enum ShouldStartReqResult {
    DO_NOT_START_REQUEST_AND_STOP_SEARCHING,
    DO_NOT_START_REQUEST_AND_KEEP_SEARCHING,
    START_REQUEST,
  };

  void InsertInFlightRequest(ScheduledResourceRequest* request) {
    in_flight_requests_.insert(request);
    SetRequestClassification(request, ClassifyRequest(request));
  }

  void EraseInFlightRequest(ScheduledResourceRequest* request) {
    size_t erased = in_flight_requests_.erase(request);
    DCHECK_EQ(1u, erased);
    // Clear any special state that we were tracking for this request.
    SetRequestClassification(request, NORMAL_REQUEST);
  }

  void ClearInFlightRequests() {
    in_flight_requests_.clear();
    in_flight_delayable_count_ = 0;
    total_layout_blocking_count_ = 0;
  }

  size_t CountRequestsWithClassification(
      const RequestClassification classification, const bool include_pending) {
    size_t classification_request_count = 0;
    for (RequestSet::const_iterator it = in_flight_requests_.begin();
         it != in_flight_requests_.end(); ++it) {
      if ((*it)->classification() == classification)
        classification_request_count++;
    }
    if (include_pending) {
      for (RequestQueue::NetQueue::const_iterator
           it = pending_requests_.GetNextHighestIterator();
           it != pending_requests_.End(); ++it) {
        if ((*it)->classification() == classification)
          classification_request_count++;
      }
    }
    return classification_request_count;
  }

  void SetRequestClassification(ScheduledResourceRequest* request,
                                RequestClassification classification) {
    RequestClassification old_classification = request->classification();
    if (old_classification == classification)
      return;

    if (old_classification == IN_FLIGHT_DELAYABLE_REQUEST)
      in_flight_delayable_count_--;
    if (old_classification == LAYOUT_BLOCKING_REQUEST)
      total_layout_blocking_count_--;

    if (classification == IN_FLIGHT_DELAYABLE_REQUEST)
      in_flight_delayable_count_++;
    if (classification == LAYOUT_BLOCKING_REQUEST)
      total_layout_blocking_count_++;

    request->set_classification(classification);
    DCHECK_EQ(
        CountRequestsWithClassification(IN_FLIGHT_DELAYABLE_REQUEST, false),
        in_flight_delayable_count_);
    DCHECK_EQ(CountRequestsWithClassification(LAYOUT_BLOCKING_REQUEST, true),
              total_layout_blocking_count_);
  }

  RequestClassification ClassifyRequest(ScheduledResourceRequest* request) {
    // If a request is already marked as layout-blocking make sure to keep the
    // classification across redirects unless the priority was lowered.
    if (request->classification() == LAYOUT_BLOCKING_REQUEST &&
        request->url_request()->priority() >= net::LOW) {
      return LAYOUT_BLOCKING_REQUEST;
    }

    if (!has_body_ && request->url_request()->priority() >= net::LOW)
      return LAYOUT_BLOCKING_REQUEST;

    if (request->url_request()->priority() < net::LOW) {
      net::HostPortPair host_port_pair =
          net::HostPortPair::FromURL(request->url_request()->url());
      net::HttpServerProperties& http_server_properties =
          *request->url_request()->context()->http_server_properties();
      if (!http_server_properties.SupportsSpdy(host_port_pair) &&
          ContainsKey(in_flight_requests_, request)) {
        return IN_FLIGHT_DELAYABLE_REQUEST;
      }
    }
    return NORMAL_REQUEST;
  }

  bool ShouldKeepSearching(
      const net::HostPortPair& active_request_host) const {
    size_t same_host_count = 0;
    for (RequestSet::const_iterator it = in_flight_requests_.begin();
         it != in_flight_requests_.end(); ++it) {
      net::HostPortPair host_port_pair =
          net::HostPortPair::FromURL((*it)->url_request()->url());
      if (active_request_host.Equals(host_port_pair)) {
        same_host_count++;
        if (same_host_count >= kMaxNumDelayableRequestsPerHost)
          return true;
      }
    }
    return false;
  }

  void StartRequest(ScheduledResourceRequest* request) {
    InsertInFlightRequest(request);
    request->Start();
  }

  // ShouldStartRequest is the main scheduling algorithm.
  //
  // Requests are evaluated on five attributes:
  //
  // 1. Non-delayable requests:
  //   * Synchronous requests.
  //   * Non-HTTP[S] requests.
  //
  // 2. Requests to SPDY-capable origin servers.
  //
  // 3. High-priority requests:
  //   * Higher priority requests (>= net::LOW).
  //
  // 4. Layout-blocking requests:
  //   * High-priority requests initiated before the renderer has a <body>.
  //
  // 5. Low priority requests
  //
  //  The following rules are followed:
  //
  //  ACTIVE_AND_LOADING and UNTHROTTLED Clients follow these rules:
  //   * Non-delayable, High-priority and SPDY capable requests are issued
  //     immediately.
  //   * Low priority requests are delayable.
  //   * Allow one delayable request to load at a time while layout-blocking
  //     requests are loading.
  //   * If no high priority or layout-blocking requests are in flight, start
  //     loading delayable requests.
  //   * Never exceed 10 delayable requests in flight per client.
  //   * Never exceed 6 delayable requests for a given host.
  //
  //  THROTTLED Clients follow these rules:
  //   * Non-delayable and SPDY-capable requests are issued immediately.
  //   * At most one non-SPDY request will be issued per THROTTLED Client
  //   * If no high priority requests are in flight, start loading low priority
  //     requests.
  //
  //  COALESCED Clients never load requests, with the following exceptions:
  //   * Non-delayable requests are issued imediately.
  //   * On a (currently 5 second) heart beat, they load all requests as an
  //     UNTHROTTLED Client, and then return to the COALESCED state.
  //   * When an active Client makes a request, they are THROTTLED until the
  //     active Client finishes loading.
  ShouldStartReqResult ShouldStartRequest(
      ScheduledResourceRequest* request) const {
    const net::URLRequest& url_request = *request->url_request();
    // Syncronous requests could block the entire render, which could impact
    // user-observable Clients.
    if (!ResourceRequestInfo::ForRequest(&url_request)->IsAsync()) {
      return START_REQUEST;
    }

    // TODO(simonjam): This may end up causing disk contention. We should
    // experiment with throttling if that happens.
    // TODO(aiolos): We probably want to Coalesce these as well to avoid
    // waking the disk.
    if (!url_request.url().SchemeIsHTTPOrHTTPS()) {
      return START_REQUEST;
    }

    if (throttle_state_ == COALESCED) {
      return DO_NOT_START_REQUEST_AND_STOP_SEARCHING;
    }

    if (using_spdy_proxy_ && url_request.url().SchemeIs("http")) {
      return START_REQUEST;
    }

    net::HostPortPair host_port_pair =
        net::HostPortPair::FromURL(url_request.url());
    net::HttpServerProperties& http_server_properties =
        *url_request.context()->http_server_properties();

    // TODO(willchan): We should really improve this algorithm as described in
    // crbug.com/164101. Also, theoretically we should not count a SPDY request
    // against the delayable requests limit.
    if (http_server_properties.SupportsSpdy(host_port_pair)) {
      return START_REQUEST;
    }

    if (throttle_state_ == THROTTLED &&
        in_flight_requests_.size() >= kMaxNumThrottledRequestsPerClient) {
      // There may still be SPDY-capable requests that should be issued.
      return DO_NOT_START_REQUEST_AND_KEEP_SEARCHING;
    }

    // High-priority and layout-blocking requests.
    if (url_request.priority() >= net::LOW) {
      return START_REQUEST;
    }

    if (in_flight_delayable_count_ >= kMaxNumDelayableRequestsPerClient) {
      return DO_NOT_START_REQUEST_AND_STOP_SEARCHING;
    }

    if (ShouldKeepSearching(host_port_pair)) {
      // There may be other requests for other hosts we'd allow,
      // so keep checking.
      return DO_NOT_START_REQUEST_AND_KEEP_SEARCHING;
    }

    bool have_immediate_requests_in_flight =
        in_flight_requests_.size() > in_flight_delayable_count_;
    if (have_immediate_requests_in_flight &&
        total_layout_blocking_count_ != 0 &&
        in_flight_delayable_count_ != 0) {
      return DO_NOT_START_REQUEST_AND_STOP_SEARCHING;
    }

    return START_REQUEST;
  }

  void LoadAnyStartablePendingRequests() {
    // We iterate through all the pending requests, starting with the highest
    // priority one. For each entry, one of three things can happen:
    // 1) We start the request, remove it from the list, and keep checking.
    // 2) We do NOT start the request, but ShouldStartRequest() signals us that
    //     there may be room for other requests, so we keep checking and leave
    //     the previous request still in the list.
    // 3) We do not start the request, same as above, but StartRequest() tells
    //     us there's no point in checking any further requests.
    RequestQueue::NetQueue::iterator request_iter =
        pending_requests_.GetNextHighestIterator();

    while (request_iter != pending_requests_.End()) {
      ScheduledResourceRequest* request = *request_iter;
      ShouldStartReqResult query_result = ShouldStartRequest(request);

      if (query_result == START_REQUEST) {
        pending_requests_.Erase(request);
        StartRequest(request);

        // StartRequest can modify the pending list, so we (re)start evaluation
        // from the currently highest priority request. Avoid copying a singular
        // iterator, which would trigger undefined behavior.
        if (pending_requests_.GetNextHighestIterator() ==
            pending_requests_.End())
          break;
        request_iter = pending_requests_.GetNextHighestIterator();
      } else if (query_result == DO_NOT_START_REQUEST_AND_KEEP_SEARCHING) {
        ++request_iter;
        continue;
      } else {
        DCHECK(query_result == DO_NOT_START_REQUEST_AND_STOP_SEARCHING);
        break;
      }
    }
  }

  bool is_audible_;
  bool is_visible_;
  bool is_loaded_;
  bool is_paused_;
  bool has_body_;
  bool using_spdy_proxy_;
  RequestQueue pending_requests_;
  RequestSet in_flight_requests_;
  ResourceScheduler* scheduler_;
  // The number of delayable in-flight requests.
  size_t in_flight_delayable_count_;
  // The number of layout-blocking in-flight requests.
  size_t total_layout_blocking_count_;
  ResourceScheduler::ClientThrottleState throttle_state_;
};

ResourceScheduler::ResourceScheduler()
    : should_coalesce_(false),
      should_throttle_(false),
      active_clients_loading_(0),
      coalesced_clients_(0),
      coalescing_timer_(new base::Timer(true /* retain_user_task */,
                                        true /* is_repeating */)) {
}

ResourceScheduler::~ResourceScheduler() {
  DCHECK(unowned_requests_.empty());
  DCHECK(client_map_.empty());
}

void ResourceScheduler::SetThrottleOptionsForTesting(bool should_throttle,
                                                     bool should_coalesce) {
  should_coalesce_ = should_coalesce;
  should_throttle_ = should_throttle;
  OnLoadingActiveClientsStateChangedForAllClients();
}

ResourceScheduler::ClientThrottleState
ResourceScheduler::GetClientStateForTesting(int child_id, int route_id) {
  Client* client = GetClient(child_id, route_id);
  DCHECK(client);
  return client->throttle_state();
}

scoped_ptr<ResourceThrottle> ResourceScheduler::ScheduleRequest(
    int child_id,
    int route_id,
    net::URLRequest* url_request) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);
  scoped_ptr<ScheduledResourceRequest> request(
      new ScheduledResourceRequest(client_id, url_request, this,
          RequestPriorityParams(url_request->priority(), 0)));

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
  client->ScheduleRequest(url_request, request.get());
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
  client->RemoveRequest(request);
}

void ResourceScheduler::OnClientCreated(int child_id, int route_id) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);
  DCHECK(!ContainsKey(client_map_, client_id));

  Client* client = new Client(this);
  client_map_[client_id] = client;

  // TODO(aiolos): set Client visibility/audibility when signals are added
  // this will UNTHROTTLE Clients as needed
  client->UpdateThrottleState();
}

void ResourceScheduler::OnClientDeleted(int child_id, int route_id) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);
  DCHECK(ContainsKey(client_map_, client_id));
  ClientMap::iterator it = client_map_.find(client_id);
  if (it == client_map_.end())
    return;

  Client* client = it->second;
  // FYI, ResourceDispatcherHost cancels all of the requests after this function
  // is called. It should end up canceling all of the requests except for a
  // cross-renderer navigation.
  RequestSet client_unowned_requests = client->RemoveAllRequests();
  for (RequestSet::iterator it = client_unowned_requests.begin();
       it != client_unowned_requests.end(); ++it) {
    unowned_requests_.insert(*it);
  }

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
  client->OnNavigate();
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
  client->OnWillInsertBody();
}

void ResourceScheduler::OnReceivedSpdyProxiedHttpResponse(
    int child_id,
    int route_id) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);

  ClientMap::iterator client_it = client_map_.find(client_id);
  if (client_it == client_map_.end()) {
    return;
  }

  Client* client = client_it->second;
  client->OnReceivedSpdyProxiedHttpResponse();
}

void ResourceScheduler::OnAudibilityChanged(int child_id,
                                            int route_id,
                                            bool is_audible) {
  Client* client = GetClient(child_id, route_id);
  DCHECK(client);
  client->OnAudibilityChanged(is_audible);
}

void ResourceScheduler::OnVisibilityChanged(int child_id,
                                            int route_id,
                                            bool is_visible) {
  Client* client = GetClient(child_id, route_id);
  DCHECK(client);
  client->OnVisibilityChanged(is_visible);
}

void ResourceScheduler::OnLoadingStateChanged(int child_id,
                                              int route_id,
                                              bool is_loaded) {
  Client* client = GetClient(child_id, route_id);
  DCHECK(client);
  client->OnLoadingStateChanged(is_loaded);
}

ResourceScheduler::Client* ResourceScheduler::GetClient(int child_id,
                                                        int route_id) {
  ClientId client_id = MakeClientId(child_id, route_id);
  ClientMap::iterator client_it = client_map_.find(client_id);
  if (client_it == client_map_.end()) {
    return NULL;
  }
  return client_it->second;
}

void ResourceScheduler::DecrementActiveClientsLoading() {
  DCHECK_NE(0u, active_clients_loading_);
  --active_clients_loading_;
  DCHECK_EQ(active_clients_loading_, CountActiveClientsLoading());
  if (active_clients_loading_ == 0) {
    OnLoadingActiveClientsStateChangedForAllClients();
  }
}

void ResourceScheduler::IncrementActiveClientsLoading() {
  ++active_clients_loading_;
  DCHECK_EQ(active_clients_loading_, CountActiveClientsLoading());
  if (active_clients_loading_ == 1) {
    OnLoadingActiveClientsStateChangedForAllClients();
  }
}

void ResourceScheduler::OnLoadingActiveClientsStateChangedForAllClients() {
  ClientMap::iterator client_it = client_map_.begin();
  while (client_it != client_map_.end()) {
    Client* client = client_it->second;
    client->UpdateThrottleState();
    ++client_it;
  }
}

size_t ResourceScheduler::CountActiveClientsLoading() const {
  size_t active_and_loading = 0;
  ClientMap::const_iterator client_it = client_map_.begin();
  while (client_it != client_map_.end()) {
    Client* client = client_it->second;
    if (client->throttle_state() == ACTIVE_AND_LOADING) {
      ++active_and_loading;
    }
    ++client_it;
  }
  return active_and_loading;
}

void ResourceScheduler::IncrementCoalescedClients() {
  ++coalesced_clients_;
  DCHECK(should_coalesce_);
  DCHECK_EQ(coalesced_clients_, CountCoalescedClients());
  if (coalesced_clients_ == 1) {
    coalescing_timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kCoalescedTimerPeriod),
        base::Bind(&ResourceScheduler::LoadCoalescedRequests,
                   base::Unretained(this)));
  }
}

void ResourceScheduler::DecrementCoalescedClients() {
  DCHECK(should_coalesce_);
  DCHECK_NE(0U, coalesced_clients_);
  --coalesced_clients_;
  DCHECK_EQ(coalesced_clients_, CountCoalescedClients());
  if (coalesced_clients_ == 0) {
    coalescing_timer_->Stop();
  }
}

size_t ResourceScheduler::CountCoalescedClients() const {
  DCHECK(should_coalesce_);
  size_t coalesced_clients = 0;
  ClientMap::const_iterator client_it = client_map_.begin();
  while (client_it != client_map_.end()) {
    Client* client = client_it->second;
    if (client->throttle_state() == COALESCED) {
      ++coalesced_clients;
    }
    ++client_it;
  }
  return coalesced_clients_;
}

void ResourceScheduler::LoadCoalescedRequests() {
  DCHECK(should_coalesce_);
  ClientMap::iterator client_it = client_map_.begin();
  while (client_it != client_map_.end()) {
    Client* client = client_it->second;
    client->LoadCoalescedRequests();
    ++client_it;
  }
}

void ResourceScheduler::ReprioritizeRequest(ScheduledResourceRequest* request,
                                            net::RequestPriority new_priority,
                                            int new_intra_priority_value) {
  if (request->url_request()->load_flags() & net::LOAD_IGNORE_LIMITS) {
    // We should not be re-prioritizing requests with the
    // IGNORE_LIMITS flag.
    NOTREACHED();
    return;
  }
  RequestPriorityParams new_priority_params(new_priority,
      new_intra_priority_value);
  RequestPriorityParams old_priority_params =
      request->get_request_priority_params();

  DCHECK(old_priority_params != new_priority_params);

  ClientMap::iterator client_it = client_map_.find(request->client_id());
  if (client_it == client_map_.end()) {
    // The client was likely deleted shortly before we received this IPC.
    request->url_request()->SetPriority(new_priority_params.priority);
    request->set_request_priority_params(new_priority_params);
    return;
  }

  if (old_priority_params == new_priority_params)
    return;

  Client *client = client_it->second;
  client->ReprioritizeRequest(
      request, old_priority_params, new_priority_params);
}

ResourceScheduler::ClientId ResourceScheduler::MakeClientId(
    int child_id, int route_id) {
  return (static_cast<ResourceScheduler::ClientId>(child_id) << 32) | route_id;
}

}  // namespace content
