// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/monitor.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

bool OnIOThread() {
  return content::BrowserThread::CurrentlyOn(content::BrowserThread::IO);
}

}  // namespace

namespace domain_reliability {

DomainReliabilityMonitor::DomainReliabilityMonitor()
    : time_(new ActualTime()) {
  DCHECK(OnIOThread());
}

DomainReliabilityMonitor::DomainReliabilityMonitor(
    scoped_ptr<MockableTime> time)
    : time_(time.Pass()) {
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
  OnRequestLegComplete(request_info);
}

DomainReliabilityContext* DomainReliabilityMonitor::AddContextForTesting(
    scoped_ptr<const DomainReliabilityConfig> config) {
  DomainReliabilityContext*& context_ref = contexts_[config->domain];
  DCHECK(!context_ref);
  context_ref = new DomainReliabilityContext(time_.get(), config.Pass());
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

void DomainReliabilityMonitor::OnRequestLegComplete(
    const RequestInfo& request) {
  if (request.was_cached ||
      request.status.status() == net::URLRequestStatus::CANCELED) {
    return;
  }

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
  beacon.elapsed = time_->Now() - beacon.start_time;
  context->AddBeacon(beacon, request.url);
}

}  // namespace domain_reliability
