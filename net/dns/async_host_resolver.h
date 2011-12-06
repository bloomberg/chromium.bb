// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_ASYNC_HOST_RESOLVER_H_
#define NET_DNS_ASYNC_HOST_RESOLVER_H_
#pragma once

#include <list>
#include <map>
#include <string>
#include <utility>

#include "base/threading/non_thread_safe.h"
#include "net/base/address_family.h"
#include "net/base/host_cache.h"
#include "net/base/host_resolver.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_log.h"
#include "net/dns/dns_client.h"

namespace net {

class NET_EXPORT AsyncHostResolver
    : public HostResolver,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  AsyncHostResolver(size_t max_dns_requests,
                    size_t max_pending_requests,
                    HostCache* cache,
                    DnsClient* client,
                    NetLog* net_log);
  virtual ~AsyncHostResolver();

  // HostResolver interface
  virtual int Resolve(const RequestInfo& info,
                      AddressList* addresses,
                      const CompletionCallback& callback,
                      RequestHandle* out_req,
                      const BoundNetLog& source_net_log) OVERRIDE;
  virtual int ResolveFromCache(const RequestInfo& info,
                               AddressList* addresses,
                               const BoundNetLog& source_net_log) OVERRIDE;
  virtual void CancelRequest(RequestHandle req_handle) OVERRIDE;
  virtual void SetDefaultAddressFamily(AddressFamily address_family) OVERRIDE;
  virtual AddressFamily GetDefaultAddressFamily() const OVERRIDE;
  virtual HostCache* GetHostCache() OVERRIDE;

  void OnDnsRequestComplete(DnsClient::Request* request,
                            int result,
                            const DnsResponse* transaction);

 private:
  FRIEND_TEST_ALL_PREFIXES(AsyncHostResolverTest, QueuedLookup);
  FRIEND_TEST_ALL_PREFIXES(AsyncHostResolverTest, CancelPendingLookup);
  FRIEND_TEST_ALL_PREFIXES(AsyncHostResolverTest,
                           ResolverDestructionCancelsLookups);
  FRIEND_TEST_ALL_PREFIXES(AsyncHostResolverTest,
                           OverflowQueueWithLowPriorityLookup);
  FRIEND_TEST_ALL_PREFIXES(AsyncHostResolverTest,
                           OverflowQueueWithHighPriorityLookup);

  class Request;

  typedef std::pair<std::string, uint16> Key;
  typedef std::list<Request*> RequestList;
  typedef std::list<const DnsClient::Request*> DnsRequestList;
  typedef std::map<Key, RequestList> KeyRequestListMap;

  // Create a new request for the incoming Resolve() call.
  Request* CreateNewRequest(const RequestInfo& info,
                            const CompletionCallback& callback,
                            AddressList* addresses,
                            const BoundNetLog& source_net_log);

  // Called when a request has just been started.
  void OnStart(Request* request);

  // Called when a request has just completed (before its callback is run).
  void OnFinish(Request* request, int result);

  // Called when a request has been cancelled.
  void OnCancel(Request* request);

  // If there is an in-progress transaction for Request->key(), this will
  // attach |request| to the respective list.
  bool AttachToRequestList(Request* request);

  // Will start a new DNS request for |request|, will insert a new key in
  // |requestlist_map_| and append |request| to the respective list.
  int StartNewDnsRequestFor(Request* request);

  // Will enqueue |request| in |pending_requests_|.
  int Enqueue(Request* request);

  // A helper used by Enqueue to insert |request| into |pending_requests_|.
  Request* Insert(Request* request);

  // Returns the number of pending requests.
  size_t GetNumPending() const;

  // Removes and returns a pointer to the lowest/highest priority request
  // from |pending_requests_|.
  Request* RemoveLowest();
  Request* RemoveHighest();

  // Once a transaction has completed, called to start a new transaction if
  // there are pending requests.
  void ProcessPending();

  // Maximum number of concurrent DNS requests.
  size_t max_dns_requests_;

  // List of current DNS requests.
  DnsRequestList dns_requests_;

  // A map from Key to a list of requests waiting for the Key to resolve.
  KeyRequestListMap requestlist_map_;

  // Maximum number of pending requests.
  size_t max_pending_requests_;

  // Queues based on priority for putting pending requests.
  RequestList pending_requests_[NUM_PRIORITIES];

  // Cache of host resolution results.
  scoped_ptr<HostCache> cache_;

  DnsClient* client_;

  NetLog* net_log_;

  DISALLOW_COPY_AND_ASSIGN(AsyncHostResolver);
};

}  // namespace net

#endif  // NET_DNS_ASYNC_HOST_RESOLVER_H_
