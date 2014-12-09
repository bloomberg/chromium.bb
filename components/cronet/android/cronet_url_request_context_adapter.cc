// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_url_request_context_adapter.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/single_thread_task_runner.h"
#include "components/cronet/url_request_context_config.h"
#include "net/base/net_errors.h"
#include "net/base/net_log_logger.h"
#include "net/base/network_delegate_impl.h"
#include "net/cert/cert_verifier.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace {

class BasicNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  BasicNetworkDelegate() {}
  virtual ~BasicNetworkDelegate() {}

 private:
  // net::NetworkDelegate implementation.
  int OnBeforeURLRequest(net::URLRequest* request,
                         const net::CompletionCallback& callback,
                         GURL* new_url) override {
    return net::OK;
  }

  int OnBeforeSendHeaders(net::URLRequest* request,
                          const net::CompletionCallback& callback,
                          net::HttpRequestHeaders* headers) override {
    return net::OK;
  }

  void OnSendHeaders(net::URLRequest* request,
                     const net::HttpRequestHeaders& headers) override {}

  int OnHeadersReceived(
      net::URLRequest* request,
      const net::CompletionCallback& callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* _response_headers,
      GURL* allowed_unsafe_redirect_url) override {
    return net::OK;
  }

  void OnBeforeRedirect(net::URLRequest* request,
                        const GURL& new_location) override {}

  void OnResponseStarted(net::URLRequest* request) override {}

  void OnRawBytesRead(const net::URLRequest& request, int bytes_read) override {
  }

  void OnCompleted(net::URLRequest* request, bool started) override {}

  void OnURLRequestDestroyed(net::URLRequest* request) override {}

  void OnPACScriptError(int line_number, const base::string16& error) override {
  }

  NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      net::AuthCredentials* credentials) override {
    return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
  }

  bool OnCanGetCookies(const net::URLRequest& request,
                       const net::CookieList& cookie_list) override {
    return false;
  }

  bool OnCanSetCookie(const net::URLRequest& request,
                      const std::string& cookie_line,
                      net::CookieOptions* options) override {
    return false;
  }

  bool OnCanAccessFile(const net::URLRequest& request,
                       const base::FilePath& path) const override {
    return false;
  }

  bool OnCanThrottleRequest(const net::URLRequest& request) const override {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(BasicNetworkDelegate);
};

}  // namespace

namespace cronet {

CronetURLRequestContextAdapter::CronetURLRequestContextAdapter() {
}

CronetURLRequestContextAdapter::~CronetURLRequestContextAdapter() {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  StopNetLogOnNetworkThread();
}

void CronetURLRequestContextAdapter::Initialize(
    scoped_ptr<URLRequestContextConfig> config,
    const base::Closure& java_init_network_thread) {
  network_thread_ = new base::Thread("network");
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  network_thread_->StartWithOptions(options);

  GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&CronetURLRequestContextAdapter::InitializeOnNetworkThread,
                 base::Unretained(this),
                 Passed(&config),
                 java_init_network_thread));
}

void CronetURLRequestContextAdapter::InitializeOnNetworkThread(
    scoped_ptr<URLRequestContextConfig> config,
    const base::Closure& java_init_network_thread) {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  // TODO(mmenke):  Add method to have the builder enable SPDY.
  net::URLRequestContextBuilder context_builder;
  context_builder.set_network_delegate(new BasicNetworkDelegate());
  context_builder.set_proxy_config_service(
      new net::ProxyConfigServiceFixed(net::ProxyConfig()));
  config->ConfigureURLRequestContextBuilder(&context_builder);

  context_.reset(context_builder.Build());

  // Currently (circa M39) enabling QUIC requires setting probability threshold.
  if (config->enable_quic) {
    context_->http_server_properties()
        ->SetAlternateProtocolProbabilityThreshold(0.0f);
    for (auto hint = config->quic_hints.begin();
         hint != config->quic_hints.end(); ++hint) {
      const URLRequestContextConfig::QuicHint& quic_hint = **hint;
      if (quic_hint.host.empty()) {
        LOG(ERROR) << "Empty QUIC hint host: " << quic_hint.host;
        continue;
      }

      url::CanonHostInfo host_info;
      std::string canon_host(net::CanonicalizeHost(quic_hint.host, &host_info));
      if (!host_info.IsIPAddress() &&
          !net::IsCanonicalizedHostCompliant(canon_host)) {
        LOG(ERROR) << "Invalid QUIC hint host: " << quic_hint.host;
        continue;
      }

      if (quic_hint.port <= std::numeric_limits<uint16>::min() ||
          quic_hint.port > std::numeric_limits<uint16>::max()) {
        LOG(ERROR) << "Invalid QUIC hint port: "
                   << quic_hint.port;
        continue;
      }

      if (quic_hint.alternate_port <= std::numeric_limits<uint16>::min() ||
          quic_hint.alternate_port > std::numeric_limits<uint16>::max()) {
        LOG(ERROR) << "Invalid QUIC hint alternate port: "
                   << quic_hint.alternate_port;
        continue;
      }

      net::HostPortPair quic_hint_host_port_pair(canon_host,
                                                 quic_hint.port);
      context_->http_server_properties()->SetAlternateProtocol(
          quic_hint_host_port_pair,
          static_cast<uint16>(quic_hint.alternate_port),
          net::AlternateProtocol::QUIC,
          1.0f);
    }
  }

  java_init_network_thread.Run();
}

void CronetURLRequestContextAdapter::Destroy() {
  DCHECK(!GetNetworkTaskRunner()->BelongsToCurrentThread());
  // Stick network_thread_ in a local, as |this| may be destroyed from the
  // network thread before delete network_thread is called.
  base::Thread* network_thread = network_thread_;
  GetNetworkTaskRunner()->DeleteSoon(FROM_HERE, this);
  // Deleting thread stops it after all tasks are completed.
  delete network_thread;
}

net::URLRequestContext* CronetURLRequestContextAdapter::GetURLRequestContext() {
  if (!context_) {
    LOG(ERROR) << "URLRequestContext is not set up";
  }
  return context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
CronetURLRequestContextAdapter::GetNetworkTaskRunner() const {
  return network_thread_->task_runner();
}

void CronetURLRequestContextAdapter::StartNetLogToFile(
    const std::string& file_name) {
  GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &CronetURLRequestContextAdapter::StartNetLogToFileOnNetworkThread,
          base::Unretained(this),
          file_name));
}

void CronetURLRequestContextAdapter::StopNetLog() {
  GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&CronetURLRequestContextAdapter::StopNetLogOnNetworkThread,
                 base::Unretained(this)));
}

void CronetURLRequestContextAdapter::StartNetLogToFileOnNetworkThread(
    const std::string& file_name) {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  // Do nothing if already logging to a file.
  if (net_log_logger_)
    return;

  base::FilePath file_path(file_name);
  FILE* file = base::OpenFile(file_path, "w");
  if (!file)
    return;

  scoped_ptr<base::Value> constants(net::NetLogLogger::GetConstants());
  net_log_logger_.reset(new net::NetLogLogger(file, *constants));
  net_log_logger_->StartObserving(context_->net_log());
}

void CronetURLRequestContextAdapter::StopNetLogOnNetworkThread() {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  if (net_log_logger_) {
    net_log_logger_->StopObserving();
    net_log_logger_.reset();
  }
}

}  // namespace cronet
