// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_job.h"

#include "base/memory/scoped_ptr.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_protocol.h"

namespace chrome_browser_net {

DnsProbeJob::DnsProbeJob(scoped_ptr<net::DnsClient> dns_client,
                         const CallbackType& callback,
                         net::NetLog* net_log)
    : bound_net_log_(
          net::BoundNetLog::Make(net_log, net::NetLog::SOURCE_DNS_PROBER)),
      dns_client_(dns_client.Pass()),
      callback_(callback),
      probe_running_(false),
      good_status_(QUERY_UNKNOWN),
      bad_status_(QUERY_UNKNOWN) {
  DCHECK(dns_client_.get());
  DCHECK(dns_client_->GetConfig());

  // TODO(ttuttle): Pick a good random hostname for the bad case.
  //                Consider running transactions in series?
  good_transaction_ = CreateTransaction("google.com");
  bad_transaction_  = CreateTransaction("thishostname.doesnotresolve");

  // StartTransaction may call the callback synchrononously, so set these
  // before we call it.
  probe_running_ = true;
  good_status_ = QUERY_RUNNING;
  bad_status_ = QUERY_RUNNING;

  // TODO(ttuttle): Log probe started.

  StartTransaction(good_transaction_.get());
  StartTransaction(bad_transaction_.get());
}

DnsProbeJob::~DnsProbeJob() {
  // TODO(ttuttle): Cleanup?  (Transactions stop themselves on destruction.)
}

void DnsProbeJob::MaybeFinishProbe(void) {
  DCHECK(probe_running_);

  if (good_status_ == QUERY_RUNNING || bad_status_ == QUERY_RUNNING)
    return;

  probe_running_ = false;

  // TODO(ttuttle): Flesh out logic.
  if (good_status_ == QUERY_ERROR || bad_status_ == QUERY_ERROR)
    RunCallback(DNS_BROKEN);
  else
    RunCallback(DNS_WORKING);

  // TODO(ttuttle): Log probe finished.
}

scoped_ptr<net::DnsTransaction> DnsProbeJob::CreateTransaction(
    const std::string& hostname) {
  return dns_client_->GetTransactionFactory()->CreateTransaction(
      hostname,
      net::dns_protocol::kTypeA,
      base::Bind(&DnsProbeJob::OnTransactionComplete,
                 base::Unretained(this)),
      bound_net_log_);
}

void DnsProbeJob::StartTransaction(net::DnsTransaction* transaction) {
  int rv = transaction->Start();
  if (rv != net::ERR_IO_PENDING) {
    // TODO(ttuttle): Make sure this counts as unreachable, not error.
    OnTransactionComplete(transaction, rv, NULL);
  }

  // TODO(ttuttle): Log transaction started.
}

void DnsProbeJob::OnTransactionComplete(net::DnsTransaction* transaction,
                                        int net_error,
                                        const net::DnsResponse* response) {
  QueryStatus* status;

  if (transaction == good_transaction_.get()) {
    status = &good_status_;
    good_transaction_.reset();
  } else if (transaction == bad_transaction_.get()) {
    status = &bad_status_;
    bad_transaction_.reset();
  } else {
    NOTREACHED();
    return;
  }

  DCHECK_EQ(QUERY_RUNNING, *status);

  if (net_error == net::OK) {
    // TODO(ttuttle): Examine DNS response and make sure it's correct.
    *status = QUERY_CORRECT;
  } else {
    *status = QUERY_ERROR;
  }

  // TODO(ttuttle): Log transaction completed.

  MaybeFinishProbe();
}

void DnsProbeJob::RunCallback(Result result) {
  callback_.Run(this, result);
}

}  // namespace chrome_browser_net
