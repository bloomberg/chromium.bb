// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/monitor.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/domain_reliability/baked_in_configs.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace domain_reliability {

DomainReliabilityMonitor::DomainReliabilityMonitor(
    const std::string& upload_reporter_string)
    : time_(new ActualTime()),
      upload_reporter_string_(upload_reporter_string),
      scheduler_params_(
          DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults()),
      dispatcher_(time_.get()),
      weak_factory_(this) {}

DomainReliabilityMonitor::DomainReliabilityMonitor(
    const std::string& upload_reporter_string,
    scoped_ptr<MockableTime> time)
    : time_(time.Pass()),
      upload_reporter_string_(upload_reporter_string),
      scheduler_params_(
          DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults()),
      dispatcher_(time_.get()),
      weak_factory_(this) {}

DomainReliabilityMonitor::~DomainReliabilityMonitor() {
  ClearContexts();
}

void DomainReliabilityMonitor::Init(
    net::URLRequestContext* url_request_context,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  DCHECK(!thread_checker_);

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter =
      new net::TrivialURLRequestContextGetter(url_request_context,
                                              task_runner);
  Init(url_request_context_getter);
}

void DomainReliabilityMonitor::Init(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter) {
  DCHECK(!thread_checker_);

  DCHECK(url_request_context_getter->GetNetworkTaskRunner()->
         RunsTasksOnCurrentThread());

  uploader_ = DomainReliabilityUploader::Create(url_request_context_getter);
  thread_checker_.reset(new base::ThreadChecker());
}

void DomainReliabilityMonitor::AddBakedInConfigs() {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());
  base::Time now = base::Time::Now();
  for (size_t i = 0; kBakedInJsonConfigs[i]; ++i) {
    std::string json(kBakedInJsonConfigs[i]);
    scoped_ptr<const DomainReliabilityConfig> config =
        DomainReliabilityConfig::FromJSON(json);
    if (config && config->IsExpired(now)) {
      LOG(WARNING) << "Baked-in Domain Reliability config for "
                   << config->domain << " is expired.";
      continue;
    }
    AddContext(config.Pass());
  }
}

void DomainReliabilityMonitor::OnBeforeRedirect(net::URLRequest* request) {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());
  // Record the redirect itself in addition to the final request.
  OnRequestLegComplete(RequestInfo(*request));
}

void DomainReliabilityMonitor::OnCompleted(net::URLRequest* request,
                                           bool started) {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());
  if (!started)
    return;
  RequestInfo request_info(*request);
  if (request_info.AccessedNetwork()) {
    OnRequestLegComplete(request_info);
    // A request was just using the network, so now is a good time to run any
    // pending and eligible uploads.
    dispatcher_.RunEligibleTasks();
  }
}

void DomainReliabilityMonitor::ClearBrowsingData(
   DomainReliabilityClearMode mode) {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  switch (mode) {
    case CLEAR_BEACONS: {
      ContextMap::const_iterator it;
      for (it = contexts_.begin(); it != contexts_.end(); ++it)
        it->second->ClearBeacons();
      break;
    };
    case CLEAR_CONTEXTS:
      ClearContexts();
      break;
    case MAX_CLEAR_MODE:
      NOTREACHED();
  }
}

scoped_ptr<base::Value> DomainReliabilityMonitor::GetWebUIData() const {
  base::ListValue* contexts_value = new base::ListValue();
  for (ContextMap::const_iterator it = contexts_.begin();
       it != contexts_.end();
       ++it) {
    contexts_value->Append(it->second->GetWebUIData().release());
  }

  base::DictionaryValue* data_value = new base::DictionaryValue();
  data_value->Set("contexts", contexts_value);

  return scoped_ptr<base::Value>(data_value);
}

DomainReliabilityContext* DomainReliabilityMonitor::AddContextForTesting(
    scoped_ptr<const DomainReliabilityConfig> config) {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());
  return AddContext(config.Pass());
}

DomainReliabilityMonitor::RequestInfo::RequestInfo() {}

DomainReliabilityMonitor::RequestInfo::RequestInfo(
    const net::URLRequest& request)
    : url(request.url()),
      status(request.status()),
      response_info(request.response_info()),
      load_flags(request.load_flags()),
      is_upload(DomainReliabilityUploader::URLRequestIsUpload(request)) {
  request.GetLoadTimingInfo(&load_timing_info);
}

DomainReliabilityMonitor::RequestInfo::~RequestInfo() {}

bool DomainReliabilityMonitor::RequestInfo::AccessedNetwork() const {
  return status.status() != net::URLRequestStatus::CANCELED &&
     response_info.network_accessed;
}

DomainReliabilityContext* DomainReliabilityMonitor::AddContext(
    scoped_ptr<const DomainReliabilityConfig> config) {
  DCHECK(config);
  DCHECK(config->IsValid());

  // Grab a copy of the domain before transferring ownership of |config|.
  std::string domain = config->domain;

  DomainReliabilityContext* context =
      new DomainReliabilityContext(time_.get(),
                                   scheduler_params_,
                                   upload_reporter_string_,
                                   &dispatcher_,
                                   uploader_.get(),
                                   config.Pass());

  std::pair<ContextMap::iterator, bool> map_it =
      contexts_.insert(make_pair(domain, context));
  // Make sure the domain wasn't already in the map.
  DCHECK(map_it.second);

  return map_it.first->second;
}

void DomainReliabilityMonitor::ClearContexts() {
  STLDeleteContainerPairSecondPointers(
      contexts_.begin(), contexts_.end());
  contexts_.clear();
}

void DomainReliabilityMonitor::OnRequestLegComplete(
    const RequestInfo& request) {
  int response_code;
  if (request.response_info.headers)
    response_code = request.response_info.headers->response_code();
  else
    response_code = -1;
  std::string beacon_status;

  int error_code = net::OK;
  if (request.status.status() == net::URLRequestStatus::FAILED)
    error_code = request.status.error();

  DomainReliabilityContext* context = GetContextForHost(request.url.host());

  // Ignore requests where:
  // 1. There is no context for the request host.
  // 2. The request did not access the network.
  // 3. The request is not supposed to send cookies (to avoid associating the
  //    request with any potentially unique data in the config).
  // 4. The request was itself a Domain Reliability upload (to avoid loops).
  // 5. There is no defined beacon status for the error or HTTP response code
  //    (to avoid leaking network-local errors).
  if (!context ||
      !request.AccessedNetwork() ||
      (request.load_flags & net::LOAD_DO_NOT_SEND_COOKIES) ||
      request.is_upload ||
      !GetDomainReliabilityBeaconStatus(
          error_code, response_code, &beacon_status)) {
    return;
  }

  DomainReliabilityBeacon beacon;
  beacon.status = beacon_status;
  beacon.chrome_error = error_code;
  if (!request.response_info.was_fetched_via_proxy)
    beacon.server_ip = request.response_info.socket_address.host();
  else
    beacon.server_ip.clear();
  beacon.protocol = GetDomainReliabilityProtocol(
      request.response_info.connection_info,
      request.response_info.ssl_info.is_valid());
  beacon.http_response_code = response_code;
  beacon.start_time = request.load_timing_info.request_start;
  beacon.elapsed = time_->NowTicks() - beacon.start_time;
  context->OnBeacon(request.url, beacon);
}

// TODO(ttuttle): Keep a separate wildcard_contexts_ map to avoid having to
// prepend '*.' to domains.
DomainReliabilityContext* DomainReliabilityMonitor::GetContextForHost(
    const std::string& host) const {
  ContextMap::const_iterator context_it;

  context_it = contexts_.find(host);
  if (context_it != contexts_.end())
    return context_it->second;

  std::string host_with_asterisk = "*." + host;
  context_it = contexts_.find(host_with_asterisk);
  if (context_it != contexts_.end())
    return context_it->second;

  size_t dot_pos = host.find('.');
  if (dot_pos == std::string::npos)
    return NULL;

  // TODO(ttuttle): Make sure parent is not in PSL before using.

  std::string parent_with_asterisk = "*." + host.substr(dot_pos + 1);
  context_it = contexts_.find(parent_with_asterisk);
  if (context_it != contexts_.end())
    return context_it->second;

  return NULL;
}

base::WeakPtr<DomainReliabilityMonitor>
DomainReliabilityMonitor::MakeWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace domain_reliability
