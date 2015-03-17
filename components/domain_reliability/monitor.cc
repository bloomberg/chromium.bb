// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/monitor.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/profiler/scoped_tracker.h"
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
    const scoped_refptr<base::SingleThreadTaskRunner>& pref_thread,
    const scoped_refptr<base::SingleThreadTaskRunner>& network_thread)
    : time_(new ActualTime()),
      upload_reporter_string_(upload_reporter_string),
      scheduler_params_(
          DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults()),
      dispatcher_(time_.get()),
      context_manager_(this),
      pref_task_runner_(pref_thread),
      network_task_runner_(network_thread),
      moved_to_network_thread_(false),
      discard_uploads_set_(false),
      weak_factory_(this) {
  DCHECK(OnPrefThread());
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

DomainReliabilityMonitor::DomainReliabilityMonitor(
    const std::string& upload_reporter_string,
    const scoped_refptr<base::SingleThreadTaskRunner>& pref_thread,
    const scoped_refptr<base::SingleThreadTaskRunner>& network_thread,
    scoped_ptr<MockableTime> time)
    : time_(time.Pass()),
      upload_reporter_string_(upload_reporter_string),
      scheduler_params_(
          DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults()),
      dispatcher_(time_.get()),
      context_manager_(this),
      pref_task_runner_(pref_thread),
      network_task_runner_(network_thread),
      moved_to_network_thread_(false),
      discard_uploads_set_(false),
      weak_factory_(this) {
  DCHECK(OnPrefThread());
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

DomainReliabilityMonitor::~DomainReliabilityMonitor() {
  if (moved_to_network_thread_)
    DCHECK(OnNetworkThread());
  else
    DCHECK(OnPrefThread());

  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void DomainReliabilityMonitor::MoveToNetworkThread() {
  DCHECK(OnPrefThread());
  DCHECK(!moved_to_network_thread_);

  moved_to_network_thread_ = true;
}

void DomainReliabilityMonitor::InitURLRequestContext(
    net::URLRequestContext* url_request_context) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/436671 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "436671 DomainReliabilityMonitor::InitURLRequestContext"));

  DCHECK(OnNetworkThread());
  DCHECK(moved_to_network_thread_);

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter =
      new net::TrivialURLRequestContextGetter(url_request_context,
                                              network_task_runner_);
  InitURLRequestContext(url_request_context_getter);
}

void DomainReliabilityMonitor::InitURLRequestContext(
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter) {
  DCHECK(OnNetworkThread());
  DCHECK(moved_to_network_thread_);

  // Make sure the URLRequestContext actually lives on what was declared to be
  // the network thread.
  DCHECK(url_request_context_getter->GetNetworkTaskRunner()->
         RunsTasksOnCurrentThread());

  uploader_ = DomainReliabilityUploader::Create(time_.get(),
                                                url_request_context_getter);
}

void DomainReliabilityMonitor::AddBakedInConfigs() {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/436671 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "436671 DomainReliabilityMonitor::AddBakedInConfigs"));

  DCHECK(OnNetworkThread());
  DCHECK(moved_to_network_thread_);

  base::Time now = base::Time::Now();
  for (size_t i = 0; kBakedInJsonConfigs[i]; ++i) {
    base::StringPiece json(kBakedInJsonConfigs[i]);
    scoped_ptr<const DomainReliabilityConfig> config =
        DomainReliabilityConfig::FromJSON(json);
    if (!config) {
      continue;
    } else if (config->IsExpired(now)) {
      LOG(WARNING) << "Baked-in Domain Reliability config for "
                   << config->domain << " is expired.";
      continue;
    }
    context_manager_.AddContextForConfig(config.Pass());
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

void DomainReliabilityMonitor::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  last_network_change_time_ = time_->NowTicks();
}

void DomainReliabilityMonitor::ClearBrowsingData(
   DomainReliabilityClearMode mode) {
  DCHECK(OnNetworkThread());

  switch (mode) {
    case CLEAR_BEACONS:
      context_manager_.ClearBeaconsInAllContexts();
      break;
    case CLEAR_CONTEXTS:
      context_manager_.RemoveAllContexts();
      break;
    case MAX_CLEAR_MODE:
      NOTREACHED();
  }
}

scoped_ptr<base::Value> DomainReliabilityMonitor::GetWebUIData() const {
  DCHECK(OnNetworkThread());

  scoped_ptr<base::DictionaryValue> data_value(new base::DictionaryValue());
  data_value->Set("contexts", context_manager_.GetWebUIData());
  return data_value.Pass();
}

DomainReliabilityContext* DomainReliabilityMonitor::AddContextForTesting(
    scoped_ptr<const DomainReliabilityConfig> config) {
  DCHECK(OnNetworkThread());

  // TODO(vadimt): Remove ScopedTracker below once crbug.com/436671 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "436671 DomainReliabilityConfig::AddContextForConfig"));

  return context_manager_.AddContextForConfig(config.Pass());
}

scoped_ptr<DomainReliabilityContext>
DomainReliabilityMonitor::CreateContextForConfig(
    scoped_ptr<const DomainReliabilityConfig> config) {
  DCHECK(OnNetworkThread());
  DCHECK(config);
  DCHECK(config->IsValid());

  return make_scoped_ptr(new DomainReliabilityContext(
      time_.get(),
      scheduler_params_,
      upload_reporter_string_,
      &last_network_change_time_,
      &dispatcher_,
      uploader_.get(),
      config.Pass()));
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

  // Ignore requests where:
  // 1. The request did not access the network.
  // 2. The request is not supposed to send cookies (to avoid associating the
  //    request with any potentially unique data in the config).
  // 3. The request was itself a Domain Reliability upload (to avoid loops).
  // 4. There is no matching beacon status for the error or HTTP response code
  //    (to avoid leaking network-local errors).
  if (!request.AccessedNetwork() ||
      (request.load_flags & net::LOAD_DO_NOT_SEND_COOKIES) ||
      request.is_upload ||
      !GetDomainReliabilityBeaconStatus(
          error_code, response_code, &beacon_status)) {
    return;
  }

  DomainReliabilityBeacon beacon;
  beacon.status = beacon_status;
  beacon.chrome_error = error_code;
  // If the response was cached, the socket address was the address that the
  // response was originally received from, so it shouldn't be copied into the
  // beacon.
  //
  // TODO(ttuttle): Wire up a way to get the real socket address in that case.
  if (!request.response_info.was_cached &&
      !request.response_info.was_fetched_via_proxy) {
    beacon.server_ip = request.response_info.socket_address.host();
  }
  beacon.protocol = GetDomainReliabilityProtocol(
      request.response_info.connection_info,
      request.response_info.ssl_info.is_valid());
  beacon.http_response_code = response_code;
  beacon.start_time = request.load_timing_info.request_start;
  beacon.elapsed = time_->NowTicks() - beacon.start_time;
  beacon.domain = request.url.host();
  context_manager_.RouteBeacon(request.url, beacon);
}

base::WeakPtr<DomainReliabilityMonitor>
DomainReliabilityMonitor::MakeWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace domain_reliability
