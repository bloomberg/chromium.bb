// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/connectivity_checker.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chromecast/net/net_switches.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

namespace chromecast {

namespace {

// How often connectivity checks are performed in seconds
const unsigned int kConnectivityPeriodSeconds = 1;

// Number of consecutive bad responses received before connectivity status is
// changed to offline
const unsigned int kNumBadResponses = 3;

// Default url for connectivity checking.
const char kDefaultConnectivityCheckUrl[] =
    "https://clients3.google.com/generate_204";

}  // namespace

ConnectivityChecker::ConnectivityChecker(
    const scoped_refptr<base::MessageLoopProxy>& loop_proxy)
    : connectivity_observer_list_(
          new ObserverListThreadSafe<ConnectivityObserver>()),
      loop_proxy_(loop_proxy),
      connected_(false),
      bad_responses_(0) {
  DCHECK(loop_proxy_.get());
  loop_proxy->PostTask(FROM_HERE,
                       base::Bind(&ConnectivityChecker::Initialize, this));
}

void ConnectivityChecker::Initialize() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringType check_url_str =
      command_line->GetSwitchValueNative(switches::kConnectivityCheckUrl);
  connectivity_check_url_.reset(new GURL(
      check_url_str.empty() ? kDefaultConnectivityCheckUrl : check_url_str));

  net::URLRequestContextBuilder builder;
  builder.set_proxy_config_service(
      new net::ProxyConfigServiceFixed(net::ProxyConfig::CreateDirect()));
  builder.DisableHttpCache();
  url_request_context_.reset(builder.Build());

  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  loop_proxy_->PostTask(FROM_HERE,
                        base::Bind(&ConnectivityChecker::Check, this));
}

ConnectivityChecker::~ConnectivityChecker() {
  DCHECK(loop_proxy_.get());
  loop_proxy_->DeleteSoon(FROM_HERE, url_request_context_.release());
  loop_proxy_->DeleteSoon(FROM_HERE, url_request_.release());
}

void ConnectivityChecker::AddConnectivityObserver(
    ConnectivityObserver* observer) {
  connectivity_observer_list_->AddObserver(observer);
}

void ConnectivityChecker::RemoveConnectivityObserver(
    ConnectivityObserver* observer) {
  connectivity_observer_list_->RemoveObserver(observer);
}

bool ConnectivityChecker::Connected() const {
  return connected_;
}

void ConnectivityChecker::SetConnectivity(bool connected) {
  if (connected_ == connected)
    return;

  connected_ = connected;
  connectivity_observer_list_->Notify(
      FROM_HERE, &ConnectivityObserver::OnConnectivityChanged, connected);
  LOG(INFO) << "Global connection is: " << (connected ? "Up" : "Down");
}

void ConnectivityChecker::Check() {
  if (!loop_proxy_->BelongsToCurrentThread()) {
    loop_proxy_->PostTask(FROM_HERE,
                          base::Bind(&ConnectivityChecker::Check, this));
    return;
  }
  DCHECK(url_request_context_.get());

  // If url_request_ is non-null, there is already a check going on. Don't
  // start another.
  if (url_request_.get())
    return;

  VLOG(1) << "Connectivity check: url=" << *connectivity_check_url_;
  url_request_ = url_request_context_->CreateRequest(
      *connectivity_check_url_, net::MAXIMUM_PRIORITY, this, NULL);
  url_request_->set_method("HEAD");
  url_request_->Start();
}

void ConnectivityChecker::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (type == net::NetworkChangeNotifier::CONNECTION_NONE)
    SetConnectivity(false);

  Cancel();
  Check();
}

void ConnectivityChecker::OnResponseStarted(net::URLRequest* request) {
  int http_response_code =
      (request->status().is_success() &&
       request->response_info().headers.get() != NULL)
          ? request->response_info().headers->response_code()
          : net::HTTP_BAD_REQUEST;

  // Clears resources.
  url_request_.reset(NULL);  // URLRequest::Cancel() is called in destructor.

  if (http_response_code < 400) {
    VLOG(1) << "Connectivity check succeeded";
    bad_responses_ = 0;
    SetConnectivity(true);
    return;
  }

  VLOG(1) << "Connectivity check failed: " << http_response_code;
  ++bad_responses_;
  if (bad_responses_ > kNumBadResponses) {
    bad_responses_ = kNumBadResponses;
    SetConnectivity(false);
  }

  // Check again
  loop_proxy_->PostDelayedTask(
      FROM_HERE, base::Bind(&ConnectivityChecker::Check, this),
      base::TimeDelta::FromSeconds(kConnectivityPeriodSeconds));
}

void ConnectivityChecker::OnReadCompleted(net::URLRequest* request,
                                          int bytes_read) {
  NOTREACHED();
}

void ConnectivityChecker::Cancel() {
  if (url_request_.get()) {
    VLOG(2) << "Cancel connectivity check in progress";
    url_request_.reset(NULL);  // URLRequest::Cancel() is called in destructor.
  }
}

}  // namespace chromecast
