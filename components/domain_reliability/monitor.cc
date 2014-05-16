// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/monitor.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/domain_reliability/baked_in_configs.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

bool OnIOThread() {
  return content::BrowserThread::CurrentlyOn(content::BrowserThread::IO);
}

// Shamelessly stolen from net/tools/get_server_time/get_server_time.cc.
// TODO(ttuttle): Merge them, if possible.
class TrivialURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  TrivialURLRequestContextGetter(
      net::URLRequestContext* context,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
      : context_(context),
        main_task_runner_(main_task_runner) {}

  // net::URLRequestContextGetter implementation:
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE {
    return context_;
  }

  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetNetworkTaskRunner() const OVERRIDE {
    return main_task_runner_;
  }

 private:
  virtual ~TrivialURLRequestContextGetter() {}

  net::URLRequestContext* context_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

}  // namespace

namespace domain_reliability {

DomainReliabilityMonitor::DomainReliabilityMonitor(
    net::URLRequestContext* url_request_context,
    const std::string& upload_reporter_string)
    : time_(new ActualTime()),
      url_request_context_getter_(scoped_refptr<net::URLRequestContextGetter>(
          new TrivialURLRequestContextGetter(
              url_request_context,
              content::BrowserThread::GetMessageLoopProxyForThread(
                  content::BrowserThread::IO)))),
      upload_reporter_string_(upload_reporter_string),
      scheduler_params_(
          DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults()),
      dispatcher_(time_.get()),
      uploader_(
          DomainReliabilityUploader::Create(url_request_context_getter_)),
      was_cleared_(false),
      cleared_mode_(MAX_CLEAR_MODE) {
  DCHECK(OnIOThread());
}

DomainReliabilityMonitor::DomainReliabilityMonitor(
    net::URLRequestContext* url_request_context,
    const std::string& upload_reporter_string,
    scoped_ptr<MockableTime> time)
    : time_(time.Pass()),
      url_request_context_getter_(scoped_refptr<net::URLRequestContextGetter>(
          new TrivialURLRequestContextGetter(
              url_request_context,
              content::BrowserThread::GetMessageLoopProxyForThread(
                  content::BrowserThread::IO)))),
      upload_reporter_string_(upload_reporter_string),
      scheduler_params_(
          DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults()),
      dispatcher_(time_.get()),
      uploader_(
          DomainReliabilityUploader::Create(url_request_context_getter_)),
      was_cleared_(false),
      cleared_mode_(MAX_CLEAR_MODE) {
  DCHECK(OnIOThread());
}

DomainReliabilityMonitor::~DomainReliabilityMonitor() {
  DCHECK(OnIOThread());
  ClearContexts();
}

void DomainReliabilityMonitor::AddBakedInConfigs() {
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
  DCHECK(OnIOThread());
  // Record the redirect itself in addition to the final request.
  OnRequestLegComplete(RequestInfo(*request));
}

void DomainReliabilityMonitor::OnCompleted(net::URLRequest* request,
                                           bool started) {
  DCHECK(OnIOThread());
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
  DCHECK(OnIOThread());

  was_cleared_ = true;
  cleared_mode_ = mode;

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

DomainReliabilityContext* DomainReliabilityMonitor::AddContextForTesting(
    scoped_ptr<const DomainReliabilityConfig> config) {
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
  ContextMap::iterator context_it;
  std::string beacon_status;

  int error_code = net::OK;
  if (request.status.status() == net::URLRequestStatus::FAILED)
    error_code = request.status.error();

  // Ignore requests where:
  // 1. There is no context for the request host.
  // 2. The request did not access the network.
  // 3. The request is not supposed to send cookies (to avoid associating the
  //    request with any potentially unique data in the config).
  // 4. The request was itself a Domain Reliability upload (to avoid loops).
  // 5. There is no defined beacon status for the error or HTTP response code
  //    (to avoid leaking network-local errors).
  if ((context_it = contexts_.find(request.url.host())) == contexts_.end() ||
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
  beacon.http_response_code = response_code;
  beacon.start_time = request.load_timing_info.request_start;
  beacon.elapsed = time_->NowTicks() - beacon.start_time;
  context_it->second->OnBeacon(request.url, beacon);
}

}  // namespace domain_reliability
