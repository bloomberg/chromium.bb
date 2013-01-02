// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_service.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/net/dns_probe_job.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_protocol.h"

using base::FieldTrialList;
using base::StringToInt;
using net::DnsClient;
using net::DnsConfig;
using net::IPAddressNumber;
using net::IPEndPoint;
using net::ParseIPLiteralToNumber;
using net::NetworkChangeNotifier;

namespace chrome_browser_net {

namespace {

// How long the DnsProbeService will cache the probe result for.
// If it's older than this and we get a probe request, the service expires it
// and starts a new probe.
const int kMaxResultAgeMs = 5000;

// The public DNS servers used by the DnsProbeService to verify internet
// connectivity.
const char kPublicDnsPrimary[]   = "8.8.8.8";
const char kPublicDnsSecondary[] = "8.8.4.4";

IPEndPoint MakeDnsEndPoint(const std::string& dns_ip_literal) {
  IPAddressNumber dns_ip_number;
  bool rv = ParseIPLiteralToNumber(dns_ip_literal, &dns_ip_number);
  DCHECK(rv);
  return IPEndPoint(dns_ip_number, net::dns_protocol::kDefaultPort);
}

const int kAttemptsUseDefault = -1;

const char kAttemptsFieldTrialName[] = "DnsProbe-Attempts";

int GetAttemptsFromFieldTrial() {
  std::string group = FieldTrialList::FindFullName(kAttemptsFieldTrialName);
  if (group == "" || group == "default")
    return kAttemptsUseDefault;

  int attempts;
  if (!StringToInt(group, &attempts))
   return kAttemptsUseDefault;

  return attempts;
}

bool IsLocalhost(const IPAddressNumber& ip) {
  return (ip.size() == net::kIPv4AddressSize)
         && (ip[0] == 127) && (ip[1] == 0) && (ip[2] == 0) && (ip[3] == 1);
}

// The maximum number of nameservers counted in histograms.
const int kNameserverCountMax = 10;

}  // namespace

DnsProbeService::DnsProbeService()
    : system_result_(DnsProbeJob::SERVERS_UNKNOWN),
      public_result_(DnsProbeJob::SERVERS_UNKNOWN),
      state_(STATE_NO_RESULTS),
      result_(PROBE_UNKNOWN),
      dns_attempts_(GetAttemptsFromFieldTrial()) {
  NetworkChangeNotifier::AddIPAddressObserver(this);
}

DnsProbeService::~DnsProbeService() {
  NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

void DnsProbeService::ProbeDns(const DnsProbeService::CallbackType& callback) {
  callbacks_.push_back(callback);

  if (state_ == STATE_RESULTS_CACHED && ResultsExpired())
    ExpireResults();

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

void DnsProbeService::OnIPAddressChanged() {
  if (state_ == STATE_RESULTS_CACHED)
    ExpireResults();
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

  // If we can't create one or both jobs, fail the probe immediately.
  if (!system_job_.get() || !public_job_.get()) {
    system_job_.reset();
    public_job_.reset();
    state_ = STATE_RESULTS_CACHED;
    // TODO(ttuttle): Should this be BAD_CONFIG?  Currently I think it only
    // happens when the system DnsConfig has no servers.
    result_ = PROBE_UNKNOWN;
    CallCallbacks();
    return;
  }

  state_ = STATE_PROBE_RUNNING;
  probe_start_time_ = base::Time::Now();
}

void DnsProbeService::OnProbesComplete() {
  DCHECK_EQ(STATE_PROBE_RUNNING, state_);

  state_ = STATE_RESULTS_CACHED;
  result_ = EvaluateResults();

  HistogramProbes();

  CallCallbacks();
}

void DnsProbeService::HistogramProbes() const {
  DCHECK_EQ(STATE_RESULTS_CACHED, state_);
  DCHECK_NE(MAX_RESULT, result_);

  base::TimeDelta elapsed = base::Time::Now() - probe_start_time_;

  UMA_HISTOGRAM_ENUMERATION("DnsProbe.Probe.Result", result_, MAX_RESULT);
  UMA_HISTOGRAM_MEDIUM_TIMES("DnsProbe.Probe.Elapsed", elapsed);

  if (NetworkChangeNotifier::IsOffline()) {
    UMA_HISTOGRAM_ENUMERATION("DnsProbe.Probe.NcnOffline.Result",
                              result_, MAX_RESULT);
    UMA_HISTOGRAM_MEDIUM_TIMES("DnsProbe.Probe.NcnOffline.Elapsed", elapsed);
  } else {
    UMA_HISTOGRAM_ENUMERATION("DnsProbe.Probe.NcnOnline.Result",
                              result_, MAX_RESULT);
    UMA_HISTOGRAM_MEDIUM_TIMES("DnsProbe.Probe.NcnOnline.Elapsed", elapsed);
  }

  switch (result_) {
  case PROBE_UNKNOWN:
    UMA_HISTOGRAM_MEDIUM_TIMES("DnsProbe.Probe.ResultUnknown.Elapsed",
                               elapsed);
    break;
  case PROBE_NO_INTERNET:
    UMA_HISTOGRAM_MEDIUM_TIMES("DnsProbe.Probe.ResultNoInternet.Elapsed",
                               elapsed);
    break;
  case PROBE_BAD_CONFIG:
    UMA_HISTOGRAM_MEDIUM_TIMES("DnsProbe.Probe.ResultBadConfig.Elapsed",
                               elapsed);

    // Histogram some extra data to see why BAD_CONFIG is happening.
    UMA_HISTOGRAM_ENUMERATION(
        "DnsProbe.Probe.ResultBadConfig.SystemJobResult",
        system_result_,
        DnsProbeJob::MAX_RESULT);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "DnsProbe.Probe.ResultBadConfig.SystemNameserverCount",
        system_nameserver_count_,
        0, kNameserverCountMax, kNameserverCountMax + 1);
    UMA_HISTOGRAM_BOOLEAN(
        "DnsProbe.Probe.ResultBadConfig.SystemIsLocalhost",
        system_is_localhost_);
    break;
  case PROBE_NXDOMAIN:
    UMA_HISTOGRAM_MEDIUM_TIMES("DnsProbe.Probe.ResultNxdomain.Elapsed",
                               elapsed);
    break;
  case MAX_RESULT:
    NOTREACHED();
    break;
  }
}

DnsProbeService::Result DnsProbeService::EvaluateResults() const {
  DCHECK_NE(DnsProbeJob::SERVERS_UNKNOWN, system_result_);
  DCHECK_NE(DnsProbeJob::SERVERS_UNKNOWN, public_result_);

  // If the system DNS is working, assume the domain doesn't exist.
  if (system_result_ == DnsProbeJob::SERVERS_CORRECT)
    return PROBE_NXDOMAIN;

  // If the system DNS is not working but another public server is, assume the
  // DNS config is bad (or perhaps the DNS servers are down or broken).
  if (public_result_ == DnsProbeJob::SERVERS_CORRECT)
    return PROBE_BAD_CONFIG;

  // If the system DNS is not working and another public server is unreachable,
  // assume the internet connection is down (note that system DNS may be a
  // router on the LAN, so it may be reachable but returning errors.)
  if (public_result_ == DnsProbeJob::SERVERS_UNREACHABLE)
    return PROBE_NO_INTERNET;

  // Otherwise: the system DNS is not working and another public server is
  // responding but with errors or incorrect results.  This is an awkward case;
  // an invasive captive portal or a restrictive firewall may be intercepting
  // or rewriting DNS traffic, or the public server may itself be failing or
  // down.
  return PROBE_UNKNOWN;
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
  if (!dns_config.IsValid())
    return scoped_ptr<DnsProbeJob>(NULL);

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
  NetworkChangeNotifier::GetDnsConfig(config);

  // DNS probes don't need or want the suffix search list populated
  config->search.clear();

  if (dns_attempts_ != kAttemptsUseDefault)
    config->attempts = dns_attempts_;

  // Take notes in case the config turns out to be bad, so we can histogram
  // some useful data.
  system_nameserver_count_ = config->nameservers.size();
  system_is_localhost_ = (system_nameserver_count_ == 1)
                         && IsLocalhost(config->nameservers[0].address());
}

void DnsProbeService::GetPublicDnsConfig(DnsConfig* config) {
  *config = DnsConfig();

  config->nameservers.push_back(MakeDnsEndPoint(kPublicDnsPrimary));
  config->nameservers.push_back(MakeDnsEndPoint(kPublicDnsSecondary));

  if (dns_attempts_ != kAttemptsUseDefault)
    config->attempts = dns_attempts_;
}

bool DnsProbeService::ResultsExpired() {
  const base::TimeDelta kMaxResultAge =
      base::TimeDelta::FromMilliseconds(kMaxResultAgeMs);
  return base::Time::Now() - probe_start_time_ > kMaxResultAge;
}

} // namespace chrome_browser_net
