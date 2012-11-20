// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_service.h"

#include "chrome/browser/net/dns_probe_job.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_protocol.h"

using net::DnsClient;
using net::DnsConfig;
using net::IPAddressNumber;
using net::IPEndPoint;
using net::ParseIPLiteralToNumber;
using net::NetworkChangeNotifier;

namespace chrome_browser_net {

namespace {

const char* kPublicDnsPrimary   = "8.8.8.8";
const char* kPublicDnsSecondary = "8.8.4.4";

IPEndPoint MakeDnsEndPoint(const std::string& dns_ip_literal) {
  IPAddressNumber dns_ip_number;
  bool rv = ParseIPLiteralToNumber(dns_ip_literal, &dns_ip_number);
  DCHECK(rv);
  return IPEndPoint(dns_ip_number, net::dns_protocol::kDefaultPort);
}

}

DnsProbeService::DnsProbeService()
    : system_result_(DnsProbeJob::SERVERS_UNKNOWN),
      public_result_(DnsProbeJob::SERVERS_UNKNOWN),
      state_(STATE_NO_RESULTS),
      result_(PROBE_UNKNOWN) {
}

DnsProbeService::~DnsProbeService() {
}

void DnsProbeService::ProbeDns(const DnsProbeService::CallbackType& callback) {
  callbacks_.push_back(callback);

  // TODO(ttuttle): Check for cache expiration.

  switch (state_) {
  case STATE_NO_RESULTS:
    StartProbes();
    break;
  case STATE_RESULTS_CACHED:
    CallCallbacks();
    break;
  case STATE_PROBE_RUNNING:
    // do nothing; probe is already running, and will call the callback
    break;
  }
}

scoped_ptr<DnsProbeJob> DnsProbeService::CreateSystemProbeJob(
    const DnsProbeJob::CallbackType& job_callback) {
  DnsConfig system_config;
  GetSystemDnsConfig(&system_config);
  return CreateProbeJob(system_config, job_callback);
}

scoped_ptr<DnsProbeJob> DnsProbeService::CreatePublicProbeJob(
    const DnsProbeJob::CallbackType& job_callback) {
  DnsConfig public_config;
  GetPublicDnsConfig(&public_config);
  return CreateProbeJob(public_config, job_callback);
}

void DnsProbeService::ExpireResults() {
  DCHECK_EQ(STATE_RESULTS_CACHED, state_);

  state_ = STATE_NO_RESULTS;
  result_ = PROBE_UNKNOWN;
}

void DnsProbeService::StartProbes() {
  DCHECK_NE(STATE_PROBE_RUNNING, state_);
  DCHECK(!system_job_.get());
  DCHECK(!public_job_.get());

  DnsProbeJob::CallbackType job_callback =
      base::Bind(&DnsProbeService::OnProbeJobComplete,
                 base::Unretained(this));

  // TODO(ttuttle): Do we want to keep explicit flags for "job done"?
  //                Or maybe DnsProbeJob should have a "finished" flag?
  system_result_ = DnsProbeJob::SERVERS_UNKNOWN;
  public_result_ = DnsProbeJob::SERVERS_UNKNOWN;

  system_job_ = CreateSystemProbeJob(job_callback);
  public_job_ = CreatePublicProbeJob(job_callback);

  state_ = STATE_PROBE_RUNNING;
}

void DnsProbeService::OnProbesComplete() {
  DCHECK_EQ(STATE_PROBE_RUNNING, state_);

  state_ = STATE_RESULTS_CACHED;
  // TODO(ttuttle): Calculate result.
  result_ = PROBE_NXDOMAIN;

  CallCallbacks();
}

void DnsProbeService::CallCallbacks() {
  DCHECK_EQ(STATE_RESULTS_CACHED, state_);
  DCHECK(!callbacks_.empty());

  std::vector<CallbackType> callbacks = callbacks_;
  callbacks_.clear();

  for (std::vector<CallbackType>::const_iterator i = callbacks.begin();
       i != callbacks.end(); ++i) {
    i->Run(result_);
  }
}

scoped_ptr<DnsProbeJob> DnsProbeService::CreateProbeJob(
    const DnsConfig& dns_config,
    const DnsProbeJob::CallbackType& job_callback) {
  scoped_ptr<DnsClient> dns_client(DnsClient::CreateClient(NULL));
  dns_client->SetConfig(dns_config);
  return DnsProbeJob::CreateJob(dns_client.Pass(), job_callback, NULL);
}

void DnsProbeService::OnProbeJobComplete(DnsProbeJob* job,
                                         DnsProbeJob::Result result) {
  DCHECK_EQ(STATE_PROBE_RUNNING, state_);

  if (job == system_job_.get()) {
    system_result_ = result;
    system_job_.reset();
  } else if (job == public_job_.get()) {
    public_result_ = result;
    public_job_.reset();
  } else {
    NOTREACHED();
    return;
  }

  if (system_result_ != DnsProbeJob::SERVERS_UNKNOWN &&
      public_result_ != DnsProbeJob::SERVERS_UNKNOWN) {
    OnProbesComplete();
  }
}

void DnsProbeService::GetSystemDnsConfig(DnsConfig* config) {
  // TODO(ttuttle): Make sure we handle missing config properly
  NetworkChangeNotifier::GetDnsConfig(config);
}

void DnsProbeService::GetPublicDnsConfig(DnsConfig* config) {
  *config = DnsConfig();
  config->nameservers.push_back(MakeDnsEndPoint(kPublicDnsPrimary));
  config->nameservers.push_back(MakeDnsEndPoint(kPublicDnsSecondary));
}

} // namespace chrome_browser_net
