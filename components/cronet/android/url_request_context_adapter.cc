// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/url_request_context_adapter.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/single_thread_task_runner.h"
#include "components/cronet/url_request_context_config.h"
#include "net/base/net_errors.h"
#include "net/base/net_log_logger.h"
#include "net/cert/cert_verifier.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace {

class BasicNetworkDelegate : public net::NetworkDelegate {
 public:
  BasicNetworkDelegate() {}
  virtual ~BasicNetworkDelegate() {}

 private:
  // net::NetworkDelegate implementation.
  virtual int OnBeforeURLRequest(net::URLRequest* request,
                                 const net::CompletionCallback& callback,
                                 GURL* new_url) OVERRIDE {
    return net::OK;
  }

  virtual int OnBeforeSendHeaders(net::URLRequest* request,
                                  const net::CompletionCallback& callback,
                                  net::HttpRequestHeaders* headers) OVERRIDE {
    return net::OK;
  }

  virtual void OnSendHeaders(net::URLRequest* request,
                             const net::HttpRequestHeaders& headers) OVERRIDE {}

  virtual int OnHeadersReceived(
      net::URLRequest* request,
      const net::CompletionCallback& callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* _response_headers,
      GURL* allowed_unsafe_redirect_url) OVERRIDE {
    return net::OK;
  }

  virtual void OnBeforeRedirect(net::URLRequest* request,
                                const GURL& new_location) OVERRIDE {}

  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE {}

  virtual void OnRawBytesRead(const net::URLRequest& request,
                              int bytes_read) OVERRIDE {}

  virtual void OnCompleted(net::URLRequest* request, bool started) OVERRIDE {}

  virtual void OnURLRequestDestroyed(net::URLRequest* request) OVERRIDE {}

  virtual void OnPACScriptError(int line_number,
                                const base::string16& error) OVERRIDE {}

  virtual NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      net::AuthCredentials* credentials) OVERRIDE {
    return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
  }

  virtual bool OnCanGetCookies(const net::URLRequest& request,
                               const net::CookieList& cookie_list) OVERRIDE {
    return false;
  }

  virtual bool OnCanSetCookie(const net::URLRequest& request,
                              const std::string& cookie_line,
                              net::CookieOptions* options) OVERRIDE {
    return false;
  }

  virtual bool OnCanAccessFile(const net::URLRequest& request,
                               const base::FilePath& path) const OVERRIDE {
    return false;
  }

  virtual bool OnCanThrottleRequest(
      const net::URLRequest& request) const OVERRIDE {
    return false;
  }

  virtual int OnBeforeSocketStreamConnect(
      net::SocketStream* stream,
      const net::CompletionCallback& callback) OVERRIDE {
    return net::OK;
  }

  DISALLOW_COPY_AND_ASSIGN(BasicNetworkDelegate);
};

}  // namespace

namespace cronet {

URLRequestContextAdapter::URLRequestContextAdapter(
    URLRequestContextAdapterDelegate* delegate,
    std::string user_agent) {
  delegate_ = delegate;
  user_agent_ = user_agent;
}

void URLRequestContextAdapter::Initialize(
    scoped_ptr<URLRequestContextConfig> config) {
  network_thread_ = new base::Thread("network");
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  network_thread_->StartWithOptions(options);

  GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestContextAdapter::InitializeURLRequestContext,
                 this,
                 Passed(&config)));
}

void URLRequestContextAdapter::InitializeURLRequestContext(
    scoped_ptr<URLRequestContextConfig> config) {
  // TODO(mmenke):  Add method to have the builder enable SPDY.
  net::URLRequestContextBuilder context_builder;
  context_builder.set_network_delegate(new BasicNetworkDelegate());
  context_builder.set_proxy_config_service(
      new net::ProxyConfigServiceFixed(net::ProxyConfig()));
  config->ConfigureURLRequestContextBuilder(&context_builder);

  context_.reset(context_builder.Build());

  if (VLOG_IS_ON(2)) {
    net_log_observer_.reset(new NetLogObserver());
    context_->net_log()->AddThreadSafeObserver(net_log_observer_.get(),
                                               net::NetLog::LOG_ALL_BUT_BYTES);
  }

  delegate_->OnContextInitialized(this);
}

URLRequestContextAdapter::~URLRequestContextAdapter() {
  if (net_log_observer_) {
    context_->net_log()->RemoveThreadSafeObserver(net_log_observer_.get());
    net_log_observer_.reset();
  }
  StopNetLog();
  // TODO(mef): Ensure that |network_thread_| is destroyed properly.
}

const std::string& URLRequestContextAdapter::GetUserAgent(
    const GURL& url) const {
  return user_agent_;
}

net::URLRequestContext* URLRequestContextAdapter::GetURLRequestContext() {
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

void URLRequestContextAdapter::StopNetLog() {
  if (net_log_logger_) {
    net_log_logger_->StopObserving();
    net_log_logger_.reset();
  }
}

void NetLogObserver::OnAddEntry(const net::NetLog::Entry& entry) {
  VLOG(2) << "Net log entry: type=" << entry.type()
          << ", source=" << entry.source().type << ", phase=" << entry.phase();
}

}  // namespace cronet
