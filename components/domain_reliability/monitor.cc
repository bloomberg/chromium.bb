// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/monitor.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "components/domain_reliability/baked_in_configs.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace domain_reliability {

DomainReliabilityMonitor::DomainReliabilityMonitor(
    const std::string& upload_reporter_string,
    scoped_refptr<base::SingleThreadTaskRunner> pref_thread,
    scoped_refptr<base::SingleThreadTaskRunner> network_thread)
    : time_(new ActualTime()),
      upload_reporter_string_(upload_reporter_string),
      scheduler_params_(
          DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults()),
      dispatcher_(time_.get()),
      pref_task_runner_(pref_thread),
      network_task_runner_(network_thread),
      moved_to_network_thread_(false),
      discard_uploads_set_(false),
      weak_factory_(this) {
  DCHECK(OnPrefThread());
}

DomainReliabilityMonitor::DomainReliabilityMonitor(
    const std::string& upload_reporter_string,
    scoped_refptr<base::SingleThreadTaskRunner> pref_thread,
    scoped_refptr<base::SingleThreadTaskRunner> network_thread,
    scoped_ptr<MockableTime> time)
    : time_(time.Pass()),
      upload_reporter_string_(upload_reporter_string),
      scheduler_params_(
          DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults()),
      dispatcher_(time_.get()),
      pref_task_runner_(pref_thread),
      network_task_runner_(network_thread),
      moved_to_network_thread_(false),
      discard_uploads_set_(false),
      weak_factory_(this) {
  DCHECK(OnPrefThread());
}

DomainReliabilityMonitor::~DomainReliabilityMonitor() {
  if (moved_to_network_thread_)
    DCHECK(OnNetworkThread());
  else
    DCHECK(OnPrefThread());

  ClearContexts();
}

void DomainReliabilityMonitor::MoveToNetworkThread() {
  DCHECK(OnPrefThread());
  DCHECK(!moved_to_network_thread_);

  moved_to_network_thread_ = true;
}

void DomainReliabilityMonitor::InitURLRequestContext(
    net::URLRequestContext* url_request_context) {
  DCHECK(OnNetworkThread());
  DCHECK(moved_to_network_thread_);

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter =
      new net::TrivialURLRequestContextGetter(url_request_context,
                                              network_task_runner_);
  InitURLRequestContext(url_request_context_getter);
}

void DomainReliabilityMonitor::InitURLRequestContext(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter) {
  DCHECK(OnNetworkThread());
  DCHECK(moved_to_network_thread_);

  // Make sure the URLRequestContext actually lives on what was declared to be
  // the network thread.
  DCHECK(url_request_context_getter->GetNetworkTaskRunner()->
         RunsTasksOnCurrentThread());

  uploader_ = DomainReliabilityUploader::Create(url_request_context_getter);
}

void DomainReliabilityMonitor::AddBakedInConfigs() {
  DCHECK(OnNetworkThread());
  DCHECK(moved_to_network_thread_);

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

void DomainReliabilityMonitor::SetDiscardUploads(bool discard_uploads) {
  DCHECK(OnNetworkThread());
  DCHECK(moved_to_network_thread_);
  DCHECK(uploader_);

  uploader_->set_discard_uploads(discard_uploads);
  discard_uploads_set_ = true;
}

void DomainReliabilityMonitor::OnBeforeRedirect(net::URLRequest* request) {
  DCHECK(OnNetworkThread());
  DCHECK(discard_uploads_set_);

  // Record the redirect itself in addition to the final request.
  OnRequestLegComplete(RequestInfo(*request));
}

void DomainReliabilityMonitor::OnCompleted(net::URLRequest* request,
                                           bool started) {
  DCHECK(OnNetworkThread());
  DCHECK(discard_uploads_set_);

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
  DCHECK(OnNetworkThread());

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
  DCHECK(OnNetworkThread());

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
  DCHECK(OnNetworkThread());

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
  DCHECK(OnNetworkThread());
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
  // Check these again because unit tests call this directly.
  DCHECK(OnNetworkThread());
  DCHECK(discard_uploads_set_);

  int response_code;
  if (request.response_info.headers.get())
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
  beacon.domain = request.url.host();
  context->OnBeacon(request.url, beacon);
}

// TODO(ttuttle): Keep a separate wildcard_contexts_ map to avoid having to
// prepend '*.' to domains.
DomainReliabilityContext* DomainReliabilityMonitor::GetContextForHost(
    const std::string& host) const {
  DCHECK(OnNetworkThread());

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
