// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_job.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_transaction.h"

using base::TimeDelta;
using net::AddressList;
using net::BoundNetLog;
using net::DnsClient;
using net::DnsResponse;
using net::DnsTransaction;
using net::NetLog;

namespace chrome_browser_net {

namespace {

// Returns true if the given net_error indicates that we received a response
// from the DNS server containing an error, or false if the given net_error
// indicates that we never received a response.
bool DidReceiveDnsResponse(int net_error) {
  switch (net_error) {
  case net::ERR_NAME_NOT_RESOLVED:  // NXDOMAIN maps to this.
  case net::ERR_DNS_MALFORMED_RESPONSE:
  case net::ERR_DNS_SERVER_REQUIRES_TCP:
  case net::ERR_DNS_SERVER_FAILED:
  case net::ERR_DNS_SORT_ERROR:  // Can only happen if the server responds.
    return true;
  default:
    return false;
  }
}

class DnsProbeJobImpl : public DnsProbeJob {
 public:
  DnsProbeJobImpl(scoped_ptr<DnsClient> dns_client,
                  const DnsProbeJob::CallbackType& callback,
                  NetLog* net_log);
  virtual ~DnsProbeJobImpl();

 private:
  enum QueryResult {
    QUERY_UNKNOWN,
    QUERY_CORRECT,
    QUERY_INCORRECT,
    QUERY_DNS_ERROR,
    QUERY_NET_ERROR,
  };

  void Start();

  scoped_ptr<DnsTransaction> CreateTransaction(
      const std::string& hostname);
  void StartTransaction(DnsTransaction* transaction);
  // Checks that |net_error| is OK, |response| parses, and has at least one
  // address.
  QueryResult EvaluateGoodResponse(int net_error, const DnsResponse* response);
  // Checks that |net_error| is OK, |response| parses but has no addresses.
  QueryResult EvaluateBadResponse(int net_error, const DnsResponse* response);
  DnsProbeJob::Result EvaluateQueryResults();
  void OnTransactionComplete(DnsTransaction* transaction,
                             int net_error,
                             const DnsResponse* response);

  BoundNetLog bound_net_log_;
  scoped_ptr<DnsClient> dns_client_;
  const DnsProbeJob::CallbackType callback_;
  scoped_ptr<DnsTransaction> good_transaction_;
  scoped_ptr<DnsTransaction> bad_transaction_;
  bool good_running_;
  bool bad_running_;
  QueryResult good_result_;
  QueryResult bad_result_;
  base::WeakPtrFactory<DnsProbeJobImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DnsProbeJobImpl);
};

DnsProbeJobImpl::DnsProbeJobImpl(scoped_ptr<DnsClient> dns_client,
                                 const DnsProbeJob::CallbackType& callback,
                                 NetLog* net_log)
    : bound_net_log_(
          BoundNetLog::Make(net_log, NetLog::SOURCE_DNS_PROBER)),
      dns_client_(dns_client.Pass()),
      callback_(callback),
      good_running_(false),
      bad_running_(false),
      good_result_(QUERY_UNKNOWN),
      bad_result_(QUERY_UNKNOWN),
      weak_factory_(this) {
  DCHECK(dns_client_.get());
  DCHECK(dns_client_->GetConfig());

  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DnsProbeJobImpl::Start,
                 weak_factory_.GetWeakPtr()));
}

DnsProbeJobImpl::~DnsProbeJobImpl() {
}

void DnsProbeJobImpl::Start() {
  // TODO(ttuttle): Pick a good random hostname for the bad case.
  //                Consider running transactions in series?
  good_transaction_ = CreateTransaction("google.com");
  bad_transaction_ = CreateTransaction("thishostname.doesnotresolve");

  // StartTransaction may call the callback synchrononously, so set these
  // before we call it.
  good_running_ = true;
  bad_running_ = true;

  // TODO(ttuttle): Log probe started.

  StartTransaction(good_transaction_.get());
  StartTransaction(bad_transaction_.get());
}

scoped_ptr<DnsTransaction> DnsProbeJobImpl::CreateTransaction(
    const std::string& hostname) {
  return dns_client_->GetTransactionFactory()->CreateTransaction(
      hostname,
      net::dns_protocol::kTypeA,
      base::Bind(&DnsProbeJobImpl::OnTransactionComplete,
                 base::Unretained(this)),
      bound_net_log_);
}

void DnsProbeJobImpl::StartTransaction(DnsTransaction* transaction) {
  int rv = transaction->Start();
  if (rv != net::ERR_IO_PENDING) {
    // TODO(ttuttle): Make sure this counts as unreachable, not error.
    OnTransactionComplete(transaction, rv, NULL);
  }

  // TODO(ttuttle): Log transaction started.
}

DnsProbeJobImpl::QueryResult DnsProbeJobImpl::EvaluateGoodResponse(
    int net_error,
    const DnsResponse* response) {
  if (net_error != net::OK)
    return DidReceiveDnsResponse(net_error) ? QUERY_DNS_ERROR : QUERY_NET_ERROR;

  AddressList addr_list;
  TimeDelta ttl;
  DnsResponse::Result result = response->ParseToAddressList(&addr_list, &ttl);

  if (result != DnsResponse::DNS_PARSE_OK)
    return QUERY_DNS_ERROR;

  if (addr_list.empty())
    return QUERY_INCORRECT;

  return QUERY_CORRECT;
}

DnsProbeJobImpl::QueryResult DnsProbeJobImpl::EvaluateBadResponse(
    int net_error,
    const DnsResponse* response) {
  if (net_error == net::ERR_NAME_NOT_RESOLVED)  // NXDOMAIN maps to this
    return QUERY_CORRECT;

  if (net_error != net::OK)
    return DidReceiveDnsResponse(net_error) ? QUERY_DNS_ERROR : QUERY_NET_ERROR;

  return QUERY_INCORRECT;
}

DnsProbeJob::Result DnsProbeJobImpl::EvaluateQueryResults() {
  if (good_result_ == QUERY_NET_ERROR || bad_result_ == QUERY_NET_ERROR)
    return SERVERS_UNREACHABLE;

  if (good_result_ == QUERY_DNS_ERROR || bad_result_ == QUERY_DNS_ERROR)
    return SERVERS_FAILING;

  // Ignore incorrect responses to known-bad query to avoid flagging domain
  // helpers.
  if (good_result_ == QUERY_INCORRECT)
    return SERVERS_INCORRECT;

  return SERVERS_CORRECT;
}

void DnsProbeJobImpl::OnTransactionComplete(DnsTransaction* transaction,
                                            int net_error,
                                            const DnsResponse* response) {
  if (transaction == good_transaction_.get()) {
    DCHECK(good_running_);
    DCHECK_EQ(QUERY_UNKNOWN, good_result_);
    good_result_ = EvaluateGoodResponse(net_error, response);
    good_running_ = false;
  } else if (transaction == bad_transaction_.get()) {
    DCHECK(bad_running_);
    DCHECK_EQ(QUERY_UNKNOWN, bad_result_);
    bad_result_ = EvaluateBadResponse(net_error, response);
    bad_running_ = false;
  } else {
    NOTREACHED();
    return;
  }

  if (good_running_ || bad_running_)
    return;

  callback_.Run(this, EvaluateQueryResults());

  // TODO(ttuttle): Log probe finished.
}

}  // namespace

scoped_ptr<DnsProbeJob> DnsProbeJob::CreateJob(
    scoped_ptr<DnsClient> dns_client,
    const CallbackType& callback,
    NetLog* net_log) {
  return scoped_ptr<DnsProbeJob>(
      new DnsProbeJobImpl(dns_client.Pass(), callback, net_log));
}

}  // namespace chrome_browser_net
