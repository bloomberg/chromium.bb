// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/async_host_resolver.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/dns_util.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket_factory.h"

namespace net {

namespace {

// TODO(agayev): fix this when IPv6 support is added.
uint16 QueryTypeFromAddressFamily(AddressFamily address_family) {
  return kDNS_A;
}

class RequestParameters : public NetLog::EventParameters {
 public:
  RequestParameters(const HostResolver::RequestInfo& info,
                    const NetLog::Source& source)
      : info_(info), source_(source) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString("hostname", info_.host_port_pair().ToString());
    dict->SetInteger("address_family",
                     static_cast<int>(info_.address_family()));
    dict->SetBoolean("allow_cached_response", info_.allow_cached_response());
    dict->SetBoolean("is_speculative", info_.is_speculative());
    dict->SetInteger("priority", info_.priority());

    if (source_.is_valid())
      dict->Set("source_dependency", source_.ToValue());

    return dict;
  }

 private:
  const HostResolver::RequestInfo info_;
  const NetLog::Source source_;
};

}  // namespace

HostResolver* CreateAsyncHostResolver(size_t max_concurrent_resolves,
                                      const IPAddressNumber& dns_ip,
                                      NetLog* net_log) {
  size_t max_transactions = max_concurrent_resolves;
  if (max_transactions == 0)
    max_transactions = 20;
  size_t max_pending_requests = max_transactions * 100;
  HostResolver* resolver = new AsyncHostResolver(
      IPEndPoint(dns_ip, 53),
      max_transactions,
      max_pending_requests,
      base::Bind(&base::RandInt),
      HostCache::CreateDefaultCache(),
      NULL,
      net_log);
  return resolver;
}

//-----------------------------------------------------------------------------
// Every call to Resolve() results in Request object being created.  Such a
// call may complete either synchronously or asynchronously or it may get
// cancelled, which can be either through specific CancelRequest call or by
// the destruction of AsyncHostResolver, which would destruct pending or
// in-progress requests, causing them to be cancelled.  Synchronous
// resolution sets |callback_| to NULL, thus, if in the destructor we still
// have a non-NULL |callback_|, we are being cancelled.
class AsyncHostResolver::Request {
 public:
  Request(AsyncHostResolver* resolver,
          const BoundNetLog& source_net_log,
          const BoundNetLog& request_net_log,
          const HostResolver::RequestInfo& info,
          const CompletionCallback& callback,
          AddressList* addresses)
      : resolver_(resolver),
        source_net_log_(source_net_log),
        request_net_log_(request_net_log),
        info_(info),
        callback_(callback),
        addresses_(addresses),
        result_(ERR_UNEXPECTED) {
    DCHECK(addresses_);
    DCHECK(resolver_);
    resolver_->OnStart(this);
    std::string dns_name;
    if (DNSDomainFromDot(info.hostname(), &dns_name))
      key_ = Key(dns_name, QueryTypeFromAddressFamily(info.address_family()));
  }

  ~Request() {
    if (!callback_.is_null())
      resolver_->OnCancel(this);
  }

  int result() const { return result_; }
  const Key& key() const {
    DCHECK(IsValid());
    return key_;
  }
  const HostResolver::RequestInfo& info() const { return info_; }
  RequestPriority priority() const { return info_.priority(); }
  const BoundNetLog& source_net_log() const { return source_net_log_; }
  const BoundNetLog& request_net_log() const { return request_net_log_; }

  bool ResolveAsIp() {
    IPAddressNumber ip_number;
    if (!ParseIPLiteralToNumber(info_.hostname(), &ip_number))
      return false;

    if (ip_number.size() != kIPv4AddressSize) {
      result_ = ERR_NAME_NOT_RESOLVED;
    } else {
      *addresses_ = AddressList::CreateFromIPAddressWithCname(
          ip_number,
          info_.port(),
          info_.host_resolver_flags() & HOST_RESOLVER_CANONNAME);
      result_ = OK;
    }
    return true;
  }

  bool ServeFromCache() {
    HostCache* cache = resolver_->cache_.get();
    if (!cache || !info_.allow_cached_response())
      return false;

    HostCache::Key key(info_.hostname(), info_.address_family(),
                       info_.host_resolver_flags());
    const HostCache::Entry* cache_entry = cache->Lookup(
        key, base::TimeTicks::Now());
    if (cache_entry) {
      request_net_log_.AddEvent(
          NetLog::TYPE_ASYNC_HOST_RESOLVER_CACHE_HIT, NULL);
      DCHECK_EQ(OK, cache_entry->error);
      result_ = cache_entry->error;
      *addresses_ =
        CreateAddressListUsingPort(cache_entry->addrlist, info_.port());
      return true;
    }
    return false;
  }

  // Called when a request completes synchronously; we do not have an
  // AddressList argument, since in case of a successful synchronous
  // completion, either ResolveAsIp or ServerFromCache would set the
  // |addresses_| and in case of an unsuccessful synchronous completion, we
  // do not touch |addresses_|.
  void OnSyncComplete(int result) {
    callback_.Reset();
    resolver_->OnFinish(this, result);
  }

  // Called when a request completes asynchronously.
  void OnAsyncComplete(int result, const AddressList& addresses) {
    if (result == OK)
      *addresses_ = CreateAddressListUsingPort(addresses, info_.port());
    DCHECK_EQ(false, callback_.is_null());
    CompletionCallback callback = callback_;
    callback_.Reset();
    resolver_->OnFinish(this, result);
    callback.Run(result);
  }

  // Returns true if request has a validly formed hostname.
  bool IsValid() const {
    return !info_.hostname().empty() && !key_.first.empty();
  }

 private:
  AsyncHostResolver* resolver_;
  BoundNetLog source_net_log_;
  BoundNetLog request_net_log_;
  const HostResolver::RequestInfo info_;
  Key key_;
  CompletionCallback callback_;
  AddressList* addresses_;
  int result_;
};

//-----------------------------------------------------------------------------
AsyncHostResolver::AsyncHostResolver(const IPEndPoint& dns_server,
                                     size_t max_transactions,
                                     size_t max_pending_requests,
                                     const RandIntCallback& rand_int_cb,
                                     HostCache* cache,
                                     ClientSocketFactory* factory,
                                     NetLog* net_log)
    : max_transactions_(max_transactions),
      max_pending_requests_(max_pending_requests),
      dns_server_(dns_server),
      rand_int_cb_(rand_int_cb),
      cache_(cache),
      factory_(factory),
      net_log_(net_log) {
}

AsyncHostResolver::~AsyncHostResolver() {
  // Destroy request lists.
  for (KeyRequestListMap::iterator it = requestlist_map_.begin();
       it != requestlist_map_.end(); ++it)
    STLDeleteElements(&it->second);

  // Destroy transactions.
  STLDeleteElements(&transactions_);

  // Destroy pending requests.
  for (size_t i = 0; i < arraysize(pending_requests_); ++i)
    STLDeleteElements(&pending_requests_[i]);
}

int AsyncHostResolver::Resolve(const RequestInfo& info,
                               AddressList* addresses,
                               const CompletionCallback& callback,
                               RequestHandle* out_req,
                               const BoundNetLog& source_net_log) {
  DCHECK(addresses);
  DCHECK_EQ(false, callback.is_null());
  scoped_ptr<Request> request(
      CreateNewRequest(info, callback, addresses, source_net_log));

  int rv = ERR_UNEXPECTED;
  if (!request->IsValid())
    rv = ERR_NAME_NOT_RESOLVED;
  else if (request->ResolveAsIp() || request->ServeFromCache())
    rv = request->result();
  else if (AttachToRequestList(request.get()))
    rv = ERR_IO_PENDING;
  else if (transactions_.size() < max_transactions_)
    rv = StartNewTransactionFor(request.get());
  else
    rv = Enqueue(request.get());

  if (rv != ERR_IO_PENDING) {
    request->OnSyncComplete(rv);
  } else {
    Request* req = request.release();
    if (out_req)
      *out_req = reinterpret_cast<RequestHandle>(req);
  }
  return rv;
}

int AsyncHostResolver::ResolveFromCache(const RequestInfo& info,
                                        AddressList* addresses,
                                        const BoundNetLog& source_net_log) {
  scoped_ptr<Request> request(
      CreateNewRequest(info, CompletionCallback(), addresses, source_net_log));
  int rv = ERR_UNEXPECTED;
  if (!request->IsValid())
    rv = ERR_NAME_NOT_RESOLVED;
  else if (request->ResolveAsIp() || request->ServeFromCache())
    rv = request->result();
  else
    rv = ERR_DNS_CACHE_MISS;
  request->OnSyncComplete(rv);
  return rv;
}

void AsyncHostResolver::OnStart(Request* request) {
  DCHECK(request);

  request->source_net_log().BeginEvent(
      NetLog::TYPE_ASYNC_HOST_RESOLVER,
      make_scoped_refptr(new NetLogSourceParameter(
          "source_dependency", request->request_net_log().source())));
  request->request_net_log().BeginEvent(
      NetLog::TYPE_ASYNC_HOST_RESOLVER_REQUEST,
      make_scoped_refptr(new RequestParameters(
          request->info(), request->source_net_log().source())));
}

void AsyncHostResolver::OnFinish(Request* request, int result) {
  DCHECK(request);
  request->request_net_log().EndEventWithNetErrorCode(
      NetLog::TYPE_ASYNC_HOST_RESOLVER_REQUEST, result);
  request->source_net_log().EndEvent(
      NetLog::TYPE_ASYNC_HOST_RESOLVER, NULL);
}

void AsyncHostResolver::OnCancel(Request* request) {
  DCHECK(request);

  request->request_net_log().AddEvent(
      NetLog::TYPE_CANCELLED, NULL);
  request->request_net_log().EndEvent(
      NetLog::TYPE_ASYNC_HOST_RESOLVER_REQUEST, NULL);
  request->source_net_log().EndEvent(
      NetLog::TYPE_ASYNC_HOST_RESOLVER, NULL);
}

void AsyncHostResolver::CancelRequest(RequestHandle req_handle) {
  scoped_ptr<Request> request(reinterpret_cast<Request*>(req_handle));
  DCHECK(request.get());

  KeyRequestListMap::iterator it = requestlist_map_.find(request->key());
  if (it != requestlist_map_.end())
    it->second.remove(request.get());
  else
    pending_requests_[request->priority()].remove(request.get());
}

void AsyncHostResolver::SetDefaultAddressFamily(
    AddressFamily address_family) {
  NOTIMPLEMENTED();
}

AddressFamily AsyncHostResolver::GetDefaultAddressFamily() const {
  return ADDRESS_FAMILY_IPV4;
}

HostCache* AsyncHostResolver::GetHostCache() {
  return cache_.get();
}

void AsyncHostResolver::OnTransactionComplete(
    int result,
    const DnsTransaction* transaction,
    const IPAddressList& ip_addresses) {
  DCHECK(std::find(transactions_.begin(), transactions_.end(), transaction)
         != transactions_.end());
  DCHECK(requestlist_map_.find(transaction->key()) != requestlist_map_.end());

  // If by the time requests that caused |transaction| are cancelled, we do
  // not have a port number to associate with the result, therefore, we
  // assume the most common port, otherwise we use the port number of the
  // first request.
  RequestList& requests = requestlist_map_[transaction->key()];
  int port = requests.empty() ? 80 : requests.front()->info().port();

  // Run callback of every request that was depending on this transaction,
  // also notify observers.
  AddressList addrlist;
  if (result == OK)
    addrlist = AddressList::CreateFromIPAddressList(ip_addresses, port);
  for (RequestList::iterator it = requests.begin(); it != requests.end();
       ++it)
    (*it)->OnAsyncComplete(result, addrlist);

  // It is possible that the requests that caused |transaction| to be
  // created are cancelled by the time |transaction| completes.  In that
  // case |requests| would be empty.  We are knowingly throwing away the
  // result of a DNS resolution in that case, because (a) if there are no
  // requests, we do not have info to obtain a key from, (b) DnsTransaction
  // does not have info(), adding one into it just temporarily doesn't make
  // sense, since HostCache will be replaced with RR cache soon, (c)
  // recreating info from DnsTransaction::Key adds a lot of temporary
  // code/functions (like converting back from qtype to AddressFamily.)
  // Also, we only cache positive results.  All of this will change when RR
  // cache is added.
  if (result == OK && cache_.get() && !requests.empty()) {
    Request* request = requests.front();
    HostResolver::RequestInfo info = request->info();
    HostCache::Key key(
        info.hostname(), info.address_family(), info.host_resolver_flags());
    cache_->Set(key, result, addrlist, base::TimeTicks::Now());
  }

  // Cleanup requests.
  STLDeleteElements(&requests);
  requestlist_map_.erase(transaction->key());

  // Cleanup transaction and start a new one if there are pending requests.
  delete transaction;
  transactions_.remove(transaction);
  ProcessPending();
}

AsyncHostResolver::Request* AsyncHostResolver::CreateNewRequest(
    const RequestInfo& info,
    const CompletionCallback& callback,
    AddressList* addresses,
    const BoundNetLog& source_net_log) {
  BoundNetLog request_net_log = BoundNetLog::Make(net_log_,
      NetLog::SOURCE_ASYNC_HOST_RESOLVER_REQUEST);
  return new Request(
      this, source_net_log, request_net_log, info, callback, addresses);
}

bool AsyncHostResolver::AttachToRequestList(Request* request) {
  KeyRequestListMap::iterator it = requestlist_map_.find(request->key());
  if (it == requestlist_map_.end())
    return false;
  it->second.push_back(request);
  return true;
}

int AsyncHostResolver::StartNewTransactionFor(Request* request) {
  DCHECK(requestlist_map_.find(request->key()) == requestlist_map_.end());
  DCHECK(transactions_.size() < max_transactions_);

  request->request_net_log().AddEvent(
      NetLog::TYPE_ASYNC_HOST_RESOLVER_CREATE_DNS_TRANSACTION, NULL);

  requestlist_map_[request->key()].push_back(request);
  DnsTransaction* transaction = new DnsTransaction(
      dns_server_,
      request->key().first,
      request->key().second,
      rand_int_cb_,
      factory_,
      request->request_net_log(),
      net_log_);
  transaction->SetDelegate(this);
  transactions_.push_back(transaction);
  return transaction->Start();
}

int AsyncHostResolver::Enqueue(Request* request) {
  Request* evicted_request = Insert(request);
  int rv = ERR_HOST_RESOLVER_QUEUE_TOO_LARGE;
  if (evicted_request == request)
    return rv;
  if (evicted_request != NULL) {
    evicted_request->OnAsyncComplete(rv, AddressList());
    delete evicted_request;
  }
  return ERR_IO_PENDING;
}

AsyncHostResolver::Request* AsyncHostResolver::Insert(Request* request) {
  pending_requests_[request->priority()].push_back(request);
  if (GetNumPending() > max_pending_requests_) {
    Request* req = RemoveLowest();
    DCHECK(req);
    return req;
  }
  return NULL;
}

size_t AsyncHostResolver::GetNumPending() const {
  size_t num_pending = 0;
  for (size_t i = 0; i < arraysize(pending_requests_); ++i)
    num_pending += pending_requests_[i].size();
  return num_pending;
}

AsyncHostResolver::Request* AsyncHostResolver::RemoveLowest() {
  for (int i = static_cast<int>(arraysize(pending_requests_)) - 1;
       i >= 0; --i) {
    RequestList& requests = pending_requests_[i];
    if (!requests.empty()) {
      Request* request = requests.front();
      requests.pop_front();
      return request;
    }
  }
  return NULL;
}

AsyncHostResolver::Request* AsyncHostResolver::RemoveHighest() {
  for (size_t i = 0; i < arraysize(pending_requests_) - 1; ++i) {
    RequestList& requests = pending_requests_[i];
    if (!requests.empty()) {
      Request* request = requests.front();
      requests.pop_front();
      return request;
    }
  }
  return NULL;
}

void AsyncHostResolver::ProcessPending() {
  Request* request = RemoveHighest();
  if (!request)
    return;
  for (size_t i = 0; i < arraysize(pending_requests_); ++i) {
    RequestList& requests = pending_requests_[i];
    RequestList::iterator it = requests.begin();
    while (it != requests.end()) {
      if (request->key() == (*it)->key()) {
        requestlist_map_[request->key()].push_back(*it);
        it = requests.erase(it);
      } else {
        ++it;
      }
    }
  }
  StartNewTransactionFor(request);
}

}  // namespace net
