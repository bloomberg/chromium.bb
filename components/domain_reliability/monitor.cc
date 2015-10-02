// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/monitor.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "components/domain_reliability/baked_in_configs.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace domain_reliability {

namespace {

int URLRequestStatusToNetError(const net::URLRequestStatus& status) {
  switch (status.status()) {
    case net::URLRequestStatus::SUCCESS:
      return net::OK;
    case net::URLRequestStatus::IO_PENDING:
      return net::ERR_IO_PENDING;
    case net::URLRequestStatus::CANCELED:
      return net::ERR_ABORTED;
    case net::URLRequestStatus::FAILED:
      return status.error();
    default:
      NOTREACHED();
      return net::ERR_UNEXPECTED;
  }
}

// Updates the status, chrome_error, and server_ip fields of |beacon| from
// the endpoint and result of |attempt|. If there is no matching status for
// the result, returns false (which means the attempt should not result in a
// beacon being reported).
bool UpdateBeaconFromAttempt(DomainReliabilityBeacon* beacon,
                             const net::ConnectionAttempt& attempt) {
  if (!GetDomainReliabilityBeaconStatus(
          attempt.result, beacon->http_response_code, &beacon->status)) {
    return false;
  }
  beacon->chrome_error = attempt.result;
  if (!attempt.endpoint.address().empty())
    beacon->server_ip = attempt.endpoint.ToString();
  else
    beacon->server_ip = "";
  return true;
}

}  // namespace

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
  // TODO(ttuttle): Remove ScopedTracker below once crbug.com/436671 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "436671 DomainReliabilityMonitor::AddBakedInConfigs"));

  DCHECK(OnNetworkThread());
  DCHECK(moved_to_network_thread_);

  for (size_t i = 0; kBakedInJsonConfigs[i]; ++i) {
    base::StringPiece json(kBakedInJsonConfigs[i]);
    scoped_ptr<const DomainReliabilityConfig> config =
        DomainReliabilityConfig::FromJSON(json);
    if (!config)
      continue;
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
  OnRequestLegComplete(request_info);

  if (request_info.response_info.network_accessed) {
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
  request.GetConnectionAttempts(&connection_attempts);
  if (!request.GetRemoteEndpoint(&remote_endpoint))
    remote_endpoint = net::IPEndPoint();
}

DomainReliabilityMonitor::RequestInfo::~RequestInfo() {}

// static
bool DomainReliabilityMonitor::RequestInfo::ShouldReportRequest(
    const DomainReliabilityMonitor::RequestInfo& request) {
  // Don't report requests for Domain Reliability uploads or that weren't
  // supposed to send cookies.
  if (request.is_upload)
    return false;
  if (request.load_flags & net::LOAD_DO_NOT_SEND_COOKIES)
    return false;
  // Report requests that accessed the network or failed with an error code
  // that Domain Reliability is interested in.
  if (request.response_info.network_accessed)
    return true;
  if (URLRequestStatusToNetError(request.status) != net::OK)
    return true;
  return false;
}

void DomainReliabilityMonitor::OnRequestLegComplete(
    const RequestInfo& request) {
  // Check these again because unit tests call this directly.
  DCHECK(OnNetworkThread());
  DCHECK(discard_uploads_set_);

  if (!RequestInfo::ShouldReportRequest(request))
    return;

  int response_code;
  if (request.response_info.headers.get())
    response_code = request.response_info.headers->response_code();
  else
    response_code = -1;

  net::ConnectionAttempt url_request_attempt(
      request.remote_endpoint, URLRequestStatusToNetError(request.status));

  DomainReliabilityBeacon beacon;
  beacon.protocol = GetDomainReliabilityProtocol(
      request.response_info.connection_info,
      request.response_info.ssl_info.is_valid());
  beacon.http_response_code = response_code;
  beacon.start_time = request.load_timing_info.request_start;
  beacon.elapsed = time_->NowTicks() - beacon.start_time;
  beacon.was_proxied = request.response_info.was_fetched_via_proxy;
  beacon.domain = request.url.host();

  // This is not foolproof -- it's possible that we'll see the same error twice
  // (e.g. an SSL error during connection on one attempt, and then an error
  // that maps to the same code during a read).
  // TODO(ttuttle): Find a way for this code to reliably tell whether we
  // eventually established a connection or not.
  bool url_request_attempt_is_duplicate = false;
  for (const auto& attempt : request.connection_attempts) {
    if (attempt.result == url_request_attempt.result)
      url_request_attempt_is_duplicate = true;
    if (!UpdateBeaconFromAttempt(&beacon, attempt))
      continue;
    context_manager_.RouteBeacon(request.url, beacon);
  }

  if (url_request_attempt_is_duplicate)
    return;
  if (!UpdateBeaconFromAttempt(&beacon, url_request_attempt))
    return;
  context_manager_.RouteBeacon(request.url, beacon);
}

base::WeakPtr<DomainReliabilityMonitor>
DomainReliabilityMonitor::MakeWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace domain_reliability
