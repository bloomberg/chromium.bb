// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/url_request_context_adapter.h"

#include <limits>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "components/cronet/url_request_context_config.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/net_errors.h"
#include "net/base/net_log_logger.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_delegate_impl.h"
#include "net/cert/cert_verifier.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties.h"
#include "net/proxy/proxy_service.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace {

class BasicNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  BasicNetworkDelegate() {}
  ~BasicNetworkDelegate() override {}

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

  void OnRawBytesRead(const net::URLRequest& request,
                      int bytes_read) override {}

  void OnCompleted(net::URLRequest* request, bool started) override {}

  void OnURLRequestDestroyed(net::URLRequest* request) override {}

  void OnPACScriptError(int line_number,
                        const base::string16& error) override {}

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

  bool OnCanThrottleRequest(
      const net::URLRequest& request) const override {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(BasicNetworkDelegate);
};

}  // namespace

namespace cronet {

URLRequestContextAdapter::URLRequestContextAdapter(
    URLRequestContextAdapterDelegate* delegate,
    std::string user_agent)
    : load_disable_cache_(true),
      is_context_initialized_(false) {
  delegate_ = delegate;
  user_agent_ = user_agent;
}

void URLRequestContextAdapter::Initialize(
    scoped_ptr<URLRequestContextConfig> config) {
  network_thread_ = new base::Thread("network");
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  network_thread_->StartWithOptions(options);
  config_ = config.Pass();
}

void URLRequestContextAdapter::InitRequestContextOnMainThread() {
  proxy_config_service_.reset(net::ProxyService::CreateSystemProxyConfigService(
      GetNetworkTaskRunner(), NULL));
  GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestContextAdapter::InitRequestContextOnNetworkThread,
                 this));
}

void URLRequestContextAdapter::InitRequestContextOnNetworkThread() {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  DCHECK(config_);
  // TODO(mmenke):  Add method to have the builder enable SPDY.
  net::URLRequestContextBuilder context_builder;
  context_builder.set_network_delegate(new BasicNetworkDelegate());
  context_builder.set_proxy_config_service(proxy_config_service_.get());
  config_->ConfigureURLRequestContextBuilder(&context_builder);

  context_.reset(context_builder.Build());

  // Currently (circa M39) enabling QUIC requires setting probability threshold.
  if (config_->enable_quic) {
    context_->http_server_properties()
        ->SetAlternateProtocolProbabilityThreshold(0.0f);
    for (size_t hint = 0; hint < config_->quic_hints.size(); ++hint) {
      const URLRequestContextConfig::QuicHint& quic_hint =
          *config_->quic_hints[hint];
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
      net::AlternativeService alternative_service(
          net::AlternateProtocol::QUIC, "",
          static_cast<uint16>(quic_hint.alternate_port));
      context_->http_server_properties()->SetAlternativeService(
          quic_hint_host_port_pair, alternative_service, 1.0f);
    }
  }
  load_disable_cache_ = config_->load_disable_cache;
  config_.reset(NULL);

  if (VLOG_IS_ON(2)) {
    net_log_observer_.reset(new NetLogObserver());
    context_->net_log()->AddThreadSafeObserver(net_log_observer_.get(),
                                               net::NetLog::LOG_ALL_BUT_BYTES);
  }

  is_context_initialized_ = true;
  while (!tasks_waiting_for_context_.empty()) {
    tasks_waiting_for_context_.front().Run();
    tasks_waiting_for_context_.pop();
  }

  delegate_->OnContextInitialized(this);
}

void URLRequestContextAdapter::PostTaskToNetworkThread(
    const tracked_objects::Location& posted_from,
    const RunAfterContextInitTask& callback) {
  GetNetworkTaskRunner()->PostTask(
      posted_from,
      base::Bind(
          &URLRequestContextAdapter::RunTaskAfterContextInitOnNetworkThread,
          this,
          callback));
}

void URLRequestContextAdapter::RunTaskAfterContextInitOnNetworkThread(
    const RunAfterContextInitTask& callback) {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  if (is_context_initialized_) {
    callback.Run();
    return;
  }
  tasks_waiting_for_context_.push(callback);
}

URLRequestContextAdapter::~URLRequestContextAdapter() {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  if (net_log_observer_) {
    context_->net_log()->RemoveThreadSafeObserver(net_log_observer_.get());
    net_log_observer_.reset();
  }
  StopNetLogHelper();
  // TODO(mef): Ensure that |network_thread_| is destroyed properly.
}

const std::string& URLRequestContextAdapter::GetUserAgent(
    const GURL& url) const {
  return user_agent_;
}

net::URLRequestContext* URLRequestContextAdapter::GetURLRequestContext() {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  if (!context_) {
    LOG(ERROR) << "URLRequestContext is not set up";
  }
  return context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
URLRequestContextAdapter::GetNetworkTaskRunner() const {
  return network_thread_->message_loop_proxy();
}

void URLRequestContextAdapter::StartNetLogToFile(const std::string& file_name) {
  PostTaskToNetworkThread(
      FROM_HERE,
      base::Bind(
          &URLRequestContextAdapter::StartNetLogToFileHelper, this, file_name));
}

void URLRequestContextAdapter::StopNetLog() {
  PostTaskToNetworkThread(
      FROM_HERE, base::Bind(&URLRequestContextAdapter::StopNetLogHelper, this));
}

void URLRequestContextAdapter::StartNetLogToFileHelper(
    const std::string& file_name) {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  // Do nothing if already logging to a file.
  if (net_log_logger_)
    return;

  base::FilePath file_path(file_name);
  base::ScopedFILE file(base::OpenFile(file_path, "w"));
  if (!file)
    return;

  net_log_logger_.reset(new net::NetLogLogger());
  net_log_logger_->StartObserving(context_->net_log(), file.Pass(), nullptr,
                                  context_.get());
}

void URLRequestContextAdapter::StopNetLogHelper() {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  if (net_log_logger_) {
    net_log_logger_->StopObserving(context_.get());
    net_log_logger_.reset();
  }
}

void NetLogObserver::OnAddEntry(const net::NetLog::Entry& entry) {
  VLOG(2) << "Net log entry: type=" << entry.type()
          << ", source=" << entry.source().type << ", phase=" << entry.phase();
}

}  // namespace cronet
