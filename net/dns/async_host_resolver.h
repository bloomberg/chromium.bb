// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_ASYNC_HOST_RESOLVER_H_
#define NET_DNS_ASYNC_HOST_RESOLVER_H_
#pragma once

#include <list>
#include <map>

#include "base/threading/non_thread_safe.h"
#include "net/base/address_family.h"
#include "net/base/host_cache.h"
#include "net/base/host_resolver.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_log.h"
#include "net/base/rand_callback.h"
#include "net/dns/dns_transaction.h"

namespace net {

class ClientSocketFactory;

class NET_EXPORT AsyncHostResolver
    : public HostResolver,
      public DnsTransaction::Delegate,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  AsyncHostResolver(const IPEndPoint& dns_server,
                    size_t max_transactions,
                    size_t max_pending_requests_,
                    const RandIntCallback& rand_int,
                    HostCache* cache,
                    ClientSocketFactory* factory,
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

  // DnsTransaction::Delegate interface
  virtual void OnTransactionComplete(
      int result,
      const DnsTransaction* transaction,
      const IPAddressList& ip_addresses) OVERRIDE;

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

  typedef DnsTransaction::Key Key;
  typedef std::list<Request*> RequestList;
  typedef std::list<const DnsTransaction*> TransactionList;
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

  // Will start a new transaction for |request|, will insert a new key in
  // |requestlist_map_| and append |request| to the respective list.
  int StartNewTransactionFor(Request* request);

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

  // Maximum number of concurrent transactions.
  size_t max_transactions_;

  // List of current transactions.
  TransactionList transactions_;

  // A map from Key to a list of requests waiting for the Key to resolve.
  KeyRequestListMap requestlist_map_;

  // Maximum number of pending requests.
  size_t max_pending_requests_;

  // Queues based on priority for putting pending requests.
  RequestList pending_requests_[NUM_PRIORITIES];

  // DNS server to which queries will be setn.
  IPEndPoint dns_server_;

  // Callback to be passed to DnsTransaction for generating DNS query ids.
  RandIntCallback rand_int_cb_;

  // Cache of host resolution results.
  scoped_ptr<HostCache> cache_;

  // Also passed to DnsTransaction; it's a dependency injection to aid
  // testing, outside of unit tests, its value is always NULL.
  ClientSocketFactory* factory_;

  NetLog* net_log_;

  DISALLOW_COPY_AND_ASSIGN(AsyncHostResolver);
};

}  // namespace net

#endif  // NET_DNS_ASYNC_HOST_RESOLVER_H_
