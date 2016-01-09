// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/connectivity_checker_impl.h"

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
#include "net/socket/ssl_client_socket.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

namespace chromecast {

namespace {

// How often connectivity checks are performed in seconds.
const unsigned int kConnectivityPeriodSeconds = 1;

// Number of consecutive connectivity check errors before status is changed
// to offline.
const unsigned int kNumErrorsToNotifyOffline = 3;

// Request timeout value in seconds.
const unsigned int kRequestTimeoutInSeconds = 3;

// Default url for connectivity checking.
const char kDefaultConnectivityCheckUrl[] =
    "https://clients3.google.com/generate_204";

// Delay notification of network change events to smooth out rapid flipping.
// Histogram "Cast.Network.Down.Duration.In.Seconds" shows 40% of network
// downtime is less than 3 seconds.
const char kNetworkChangedDelayInSeconds = 3;

}  // namespace

ConnectivityCheckerImpl::ConnectivityCheckerImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : ConnectivityChecker(),
      task_runner_(task_runner),
      connected_(false),
      connection_type_(net::NetworkChangeNotifier::CONNECTION_NONE),
      check_errors_(0),
      network_changed_pending_(false) {
  DCHECK(task_runner_.get());
  task_runner->PostTask(FROM_HERE,
                        base::Bind(&ConnectivityCheckerImpl::Initialize, this));
}

void ConnectivityCheckerImpl::Initialize() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringType check_url_str =
      command_line->GetSwitchValueNative(switches::kConnectivityCheckUrl);
  connectivity_check_url_.reset(new GURL(
      check_url_str.empty() ? kDefaultConnectivityCheckUrl : check_url_str));

  net::URLRequestContextBuilder builder;
  builder.set_proxy_config_service(make_scoped_ptr(
      new net::ProxyConfigServiceFixed(net::ProxyConfig::CreateDirect())));
  builder.DisableHttpCache();
  url_request_context_ = builder.Build();

  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&ConnectivityCheckerImpl::Check, this));
}

ConnectivityCheckerImpl::~ConnectivityCheckerImpl() {
  DCHECK(task_runner_.get());
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  task_runner_->DeleteSoon(FROM_HERE, url_request_.release());
  task_runner_->DeleteSoon(FROM_HERE, url_request_context_.release());
}

bool ConnectivityCheckerImpl::Connected() const {
  return connected_;
}

void ConnectivityCheckerImpl::SetConnected(bool connected) {
  if (connected_ == connected)
    return;

  connected_ = connected;
  Notify(connected);
  LOG(INFO) << "Global connection is: " << (connected ? "Up" : "Down");
}

void ConnectivityCheckerImpl::Check() {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&ConnectivityCheckerImpl::Check, this));
    return;
  }
  DCHECK(url_request_context_.get());

  // Don't check connectivity if network is offline, because Internet could be
  // accessible via netifs ignored.
  if (net::NetworkChangeNotifier::IsOffline())
    return;

  // If url_request_ is non-null, there is already a check going on. Don't
  // start another.
  if (url_request_.get())
    return;

  VLOG(1) << "Connectivity check: url=" << *connectivity_check_url_;
  url_request_ = url_request_context_->CreateRequest(
      *connectivity_check_url_, net::MAXIMUM_PRIORITY, this);
  url_request_->set_method("HEAD");
  url_request_->Start();

  timeout_.Reset(base::Bind(&ConnectivityCheckerImpl::OnUrlRequestTimeout,
                            this));
  // Exponential backoff for timeout in 3, 6 and 12 sec.
  const int timeout = kRequestTimeoutInSeconds
                      << (check_errors_ > 2 ? 2 : check_errors_);
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      timeout_.callback(),
      base::TimeDelta::FromSeconds(timeout));
}

void ConnectivityCheckerImpl::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  VLOG(2) << "OnNetworkChanged " << type;
  connection_type_ = type;

  if (network_changed_pending_)
    return;
  network_changed_pending_ = true;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ConnectivityCheckerImpl::OnNetworkChangedInternal, this),
      base::TimeDelta::FromSeconds(kNetworkChangedDelayInSeconds));
}

void ConnectivityCheckerImpl::OnNetworkChangedInternal() {
  network_changed_pending_ = false;
  Cancel();

  if (connection_type_ == net::NetworkChangeNotifier::CONNECTION_NONE) {
    SetConnected(false);
    return;
  }

  Check();
}

void ConnectivityCheckerImpl::OnResponseStarted(net::URLRequest* request) {
  int http_response_code =
      (request->status().is_success() &&
       request->response_info().headers.get() != nullptr)
          ? request->response_info().headers->response_code()
          : net::HTTP_BAD_REQUEST;

  // Clears resources.
  url_request_.reset(nullptr);  // URLRequest::Cancel() is called in destructor.

  if (http_response_code < 400) {
    VLOG(1) << "Connectivity check succeeded";
    check_errors_ = 0;
    SetConnected(true);
    timeout_.Cancel();
    return;
  }
  VLOG(1) << "Connectivity check failed: " << http_response_code;
  OnUrlRequestError();
  timeout_.Cancel();
}

void ConnectivityCheckerImpl::OnReadCompleted(net::URLRequest* request,
                                              int bytes_read) {
  NOTREACHED();
}

void ConnectivityCheckerImpl::OnSSLCertificateError(
    net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  LOG(ERROR) << "OnSSLCertificateError: cert_status=" << ssl_info.cert_status;
  net::SSLClientSocket::ClearSessionCache();
  OnUrlRequestError();
  timeout_.Cancel();
}

void ConnectivityCheckerImpl::OnUrlRequestError() {
  ++check_errors_;
  if (check_errors_ > kNumErrorsToNotifyOffline) {
    check_errors_ = kNumErrorsToNotifyOffline;
    SetConnected(false);
  }
  url_request_.reset(nullptr);
  // Check again.
  task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&ConnectivityCheckerImpl::Check, this),
      base::TimeDelta::FromSeconds(kConnectivityPeriodSeconds));
}

void ConnectivityCheckerImpl::OnUrlRequestTimeout() {
  LOG(ERROR) << "time out";
  OnUrlRequestError();
}

void ConnectivityCheckerImpl::Cancel() {
  if (!url_request_.get())
    return;
  VLOG(2) << "Cancel connectivity check in progress";
  url_request_.reset(nullptr);  // URLRequest::Cancel() is called in destructor.
  timeout_.Cancel();
}

}  // namespace chromecast
