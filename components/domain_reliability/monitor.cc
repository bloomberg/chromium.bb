// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/monitor.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
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
    net::URLRequestContext* url_request_context)
    : time_(new ActualTime()),
      url_request_context_getter_(scoped_refptr<net::URLRequestContextGetter>(
          new TrivialURLRequestContextGetter(
              url_request_context,
              content::BrowserThread::GetMessageLoopProxyForThread(
                  content::BrowserThread::IO)))),
      scheduler_params_(
          DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults()),
      dispatcher_(time_.get()),
      uploader_(
          DomainReliabilityUploader::Create(url_request_context_getter_)) {
  DCHECK(OnIOThread());
}

DomainReliabilityMonitor::DomainReliabilityMonitor(
    net::URLRequestContext* url_request_context,
    scoped_ptr<MockableTime> time)
    : time_(time.Pass()),
      url_request_context_getter_(scoped_refptr<net::URLRequestContextGetter>(
          new TrivialURLRequestContextGetter(
              url_request_context,
              content::BrowserThread::GetMessageLoopProxyForThread(
                  content::BrowserThread::IO)))),
      scheduler_params_(
          DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults()),
      dispatcher_(time_.get()),
      uploader_(
          DomainReliabilityUploader::Create(url_request_context_getter_)) {
  DCHECK(OnIOThread());
}

DomainReliabilityMonitor::~DomainReliabilityMonitor() {
  DCHECK(OnIOThread());
  STLDeleteContainerPairSecondPointers(
      contexts_.begin(), contexts_.end());
}

void DomainReliabilityMonitor::OnBeforeRedirect(net::URLRequest* request) {
  DCHECK(OnIOThread());
  RequestInfo request_info(*request);
  // Record the redirect itself in addition to the final request.
  OnRequestLegComplete(request_info);
}

void DomainReliabilityMonitor::OnCompleted(net::URLRequest* request,
                                           bool started) {
  DCHECK(OnIOThread());
  if (!started)
    return;
  RequestInfo request_info(*request);
  if (request_info.DefinitelyReachedNetwork()) {
    OnRequestLegComplete(request_info);
    // A request was just using the network, so now is a good time to run any
    // pending and eligible uploads.
    dispatcher_.RunEligibleTasks();
  }
}

DomainReliabilityContext* DomainReliabilityMonitor::AddContextForTesting(
    scoped_ptr<const DomainReliabilityConfig> config) {
  DomainReliabilityContext*& context_ref = contexts_[config->domain];
  DCHECK(!context_ref);
  context_ref = new DomainReliabilityContext(
      time_.get(),
      scheduler_params_,
      &dispatcher_,
      uploader_.get(),
      config.Pass());
  return context_ref;
}

DomainReliabilityMonitor::RequestInfo::RequestInfo() {}

DomainReliabilityMonitor::RequestInfo::RequestInfo(
    const net::URLRequest& request)
    : url(request.url()),
      status(request.status()),
      response_code(-1),
      socket_address(request.GetSocketAddress()),
      was_cached(request.was_cached()) {
  request.GetLoadTimingInfo(&load_timing_info);
  // Can't get response code of a canceled request -- there's no transaction.
  if (status.status() != net::URLRequestStatus::CANCELED)
    response_code = request.GetResponseCode();
}

DomainReliabilityMonitor::RequestInfo::~RequestInfo() {}

bool DomainReliabilityMonitor::RequestInfo::DefinitelyReachedNetwork() const {
  return status.status() != net::URLRequestStatus::CANCELED && !was_cached;
}

void DomainReliabilityMonitor::OnRequestLegComplete(
    const RequestInfo& request) {
  if (!request.DefinitelyReachedNetwork())
    return;

  std::map<std::string, DomainReliabilityContext*>::iterator it =
      contexts_.find(request.url.host());
  if (it == contexts_.end())
    return;
  DomainReliabilityContext* context = it->second;

  std::string beacon_status;
  bool got_status = DomainReliabilityUtil::GetBeaconStatus(
      request.status.error(),
      request.response_code,
      &beacon_status);
  if (!got_status)
    return;

  DomainReliabilityBeacon beacon;
  beacon.status = beacon_status;
  beacon.chrome_error = request.status.error();
  beacon.server_ip = request.socket_address.host();
  beacon.http_response_code = request.response_code;
  beacon.start_time = request.load_timing_info.request_start;
  beacon.elapsed = time_->NowTicks() - beacon.start_time;
  context->AddBeacon(beacon, request.url);
}

}  // namespace domain_reliability
