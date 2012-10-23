// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_job.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_transaction.h"

using net::BoundNetLog;
using net::DnsClient;
using net::DnsResponse;
using net::DnsTransaction;
using net::NetLog;

namespace chrome_browser_net {

namespace {

class DnsProbeJobImpl : public DnsProbeJob {
 public:
  DnsProbeJobImpl(scoped_ptr<DnsClient> dns_client,
                  const DnsProbeJob::CallbackType& callback,
                  NetLog* net_log);
  virtual ~DnsProbeJobImpl();

 private:
  enum QueryStatus {
    QUERY_UNKNOWN,
    QUERY_CORRECT,
    QUERY_INCORRECT,
    QUERY_ERROR,
    QUERY_RUNNING,
  };

  void MaybeFinishProbe();
  scoped_ptr<DnsTransaction> CreateTransaction(
      const std::string& hostname);
  void StartTransaction(DnsTransaction* transaction);
  void OnTransactionComplete(DnsTransaction* transaction,
                             int net_error,
                             const DnsResponse* response);
  void RunCallback(DnsProbeJob::Result result);

  BoundNetLog bound_net_log_;
  scoped_ptr<DnsClient> dns_client_;
  const DnsProbeJob::CallbackType callback_;
  bool probe_running_;
  scoped_ptr<DnsTransaction> good_transaction_;
  scoped_ptr<DnsTransaction> bad_transaction_;
  QueryStatus good_status_;
  QueryStatus bad_status_;

  DISALLOW_COPY_AND_ASSIGN(DnsProbeJobImpl);
};

DnsProbeJobImpl::DnsProbeJobImpl(scoped_ptr<DnsClient> dns_client,
                                 const DnsProbeJob::CallbackType& callback,
                                 NetLog* net_log)
    : bound_net_log_(
          BoundNetLog::Make(net_log, NetLog::SOURCE_DNS_PROBER)),
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

DnsProbeJobImpl::~DnsProbeJobImpl() {
  // TODO(ttuttle): Cleanup?  (Transactions stop themselves on destruction.)
}

void DnsProbeJobImpl::MaybeFinishProbe(void) {
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

void DnsProbeJobImpl::OnTransactionComplete(DnsTransaction* transaction,
                                            int net_error,
                                            const DnsResponse* response) {
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

void DnsProbeJobImpl::RunCallback(DnsProbeJob::Result result) {
  // Make sure we're not running the callback in the constructor.
  // This avoids a race where our owner tries to destroy us while we're still
  // being created, then ends up with a dangling pointer to us.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback_, base::Unretained(this), result));
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
