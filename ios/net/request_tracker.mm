// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/request_tracker.h"

#include "base/logging.h"
#import "ios/net/clients/crn_forwarding_network_client.h"
#import "ios/net/clients/crn_forwarding_network_client_factory.h"

namespace net {

namespace {

// Reference to the single instance of the RequestTrackerFactory.
RequestTracker::RequestTrackerFactory* g_request_tracker_factory = nullptr;

// Array of network client factories that should be added to any new request
// tracker.
class GlobalNetworkClientFactories {
 public:
  static GlobalNetworkClientFactories* GetInstance() {
    if (!g_global_network_client_factories)
      g_global_network_client_factories = new GlobalNetworkClientFactories;
    return g_global_network_client_factories;
  }

  // Gets the factories.
  NSArray* GetFactories() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return factories_.get();
  }

  // Adds a factory.
  void AddFactory(CRNForwardingNetworkClientFactory* factory) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK_EQ(NSNotFound,
              static_cast<NSInteger>([factories_ indexOfObject:factory]));
    DCHECK(!IsSelectorOverriden(factory, @selector(clientHandlingRequest:)));
    DCHECK(!IsSelectorOverriden(factory,
                                @selector(clientHandlingResponse:request:)));
    DCHECK(!IsSelectorOverriden(
               factory, @selector(clientHandlingRedirect:url:response:)));
    [factories_ addObject:factory];
  }

  // Returns true if |factory| re-implements |selector|.
  // Only used for debugging.
  bool IsSelectorOverriden(CRNForwardingNetworkClientFactory* factory,
                           SEL selector) {
    return
        [factory methodForSelector:selector] !=
        [CRNForwardingNetworkClientFactory instanceMethodForSelector:selector];
  }

 private:
  GlobalNetworkClientFactories() : factories_([[NSMutableArray alloc] init]) {}

  base::scoped_nsobject<NSMutableArray> factories_;
  base::ThreadChecker thread_checker_;

  static GlobalNetworkClientFactories* g_global_network_client_factories;
};

GlobalNetworkClientFactories*
    GlobalNetworkClientFactories::g_global_network_client_factories = nullptr;

}  // namespace

RequestTracker::RequestTrackerFactory::~RequestTrackerFactory() {
}

// static
void RequestTracker::SetRequestTrackerFactory(RequestTrackerFactory* factory) {
  g_request_tracker_factory = factory;
}

// static
bool RequestTracker::GetRequestTracker(NSURLRequest* request,
                                       base::WeakPtr<RequestTracker>* tracker) {
  DCHECK(request);
  DCHECK(tracker);
  DCHECK(!tracker->get());
  if (!g_request_tracker_factory) {
    return true;
  }
  return g_request_tracker_factory->GetRequestTracker(request, tracker);
}

void RequestTracker::AddNetworkClientFactory(
    CRNForwardingNetworkClientFactory* factory) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK([[factory clientClass]
      isSubclassOfClass:[CRNForwardingNetworkClient class]]);
  // Sanity check: We don't already have a factory of the type being added.
  DCHECK([client_factories_ indexOfObjectPassingTest:^BOOL(id obj,
                                                           NSUInteger idx,
                                                           BOOL* stop) {
      return [obj clientClass] == [factory clientClass];
  }] == NSNotFound);
  [client_factories_ addObject:factory];
  if ([factory requiresOrdering]) {
    [client_factories_ sortUsingSelector:@selector(orderRelativeTo:)];
  }
}

// static
void RequestTracker::AddGlobalNetworkClientFactory(
    CRNForwardingNetworkClientFactory* factory) {
  GlobalNetworkClientFactories::GetInstance()->AddFactory(factory);
}

RequestTracker::RequestTracker()
    : client_factories_([[NSMutableArray alloc] init]),
      initialized_(false),
      cache_mode_(CACHE_NORMAL),
      weak_ptr_factory_(this) {
  // RequestTracker can be created from the main thread and used from another
  // thread.
  thread_checker_.DetachFromThread();
}

RequestTracker::~RequestTracker() {
}

base::WeakPtr<RequestTracker> RequestTracker::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void RequestTracker::InvalidateWeakPtrs() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void RequestTracker::Init() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!initialized_);
  initialized_ = true;
  for (CRNForwardingNetworkClientFactory* factory in
           GlobalNetworkClientFactories::GetInstance()->GetFactories()) {
    AddNetworkClientFactory(factory);
  }
}

// static
NSArray* RequestTracker::GlobalClientsHandlingAnyRequest() {
  NSMutableArray* applicable_clients = [NSMutableArray array];
  for (CRNForwardingNetworkClientFactory* factory in
           GlobalNetworkClientFactories::GetInstance()->GetFactories()) {
    CRNForwardingNetworkClient* client = [factory clientHandlingAnyRequest];
    if (client)
      [applicable_clients addObject:client];
  }
  return applicable_clients;
}

NSArray* RequestTracker::ClientsHandlingAnyRequest() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initialized_);
  NSMutableArray* applicable_clients = [NSMutableArray array];
  for (CRNForwardingNetworkClientFactory* factory in client_factories_.get()) {
    CRNForwardingNetworkClient* client = [factory clientHandlingAnyRequest];
    if (client)
      [applicable_clients addObject:client];
  }
  return applicable_clients;
}

NSArray* RequestTracker::ClientsHandlingRequest(const URLRequest& request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initialized_);
  NSMutableArray* applicable_clients = [NSMutableArray array];
  for (CRNForwardingNetworkClientFactory* factory in client_factories_.get()) {
    CRNForwardingNetworkClient* client =
        [factory clientHandlingRequest:request];
    if (client)
      [applicable_clients addObject:client];
  }
  return applicable_clients;
}

NSArray* RequestTracker::ClientsHandlingRequestAndResponse(
    const URLRequest& request,
    NSURLResponse* response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initialized_);
  NSMutableArray* applicable_clients = [NSMutableArray array];
  for (CRNForwardingNetworkClientFactory* factory in client_factories_.get()) {
    CRNForwardingNetworkClient* client =
        [factory clientHandlingResponse:response request:request];
    if (client)
      [applicable_clients addObject:client];
  }
  return applicable_clients;
}

NSArray* RequestTracker::ClientsHandlingRedirect(
    const URLRequest& request,
    const GURL& new_url,
    NSURLResponse* redirect_response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initialized_);
  NSMutableArray* applicable_clients = [NSMutableArray array];
  for (CRNForwardingNetworkClientFactory* factory in client_factories_.get()) {
    CRNForwardingNetworkClient* client =
        [factory clientHandlingRedirect:request
                                    url:new_url
                               response:redirect_response];
    if (client)
      [applicable_clients addObject:client];
  }
  return applicable_clients;
}

RequestTracker::CacheMode RequestTracker::GetCacheMode() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return cache_mode_;
}

void RequestTracker::SetCacheMode(RequestTracker::CacheMode mode) {
  DCHECK(thread_checker_.CalledOnValidThread());
  cache_mode_ = mode;
}

}  // namespace net
