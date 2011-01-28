// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_service.h"

#include "chrome/browser/browser_thread.h"
#include "net/base/cookie_monster.h"
#include "net/base/host_resolver.h"
#include "net/base/load_flags.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_layer.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/policy/device_management_backend_impl.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "webkit/glue/webkit_glue.h"

namespace policy {

namespace {

// Custom request context implementation that allows to override the user agent,
// amongst others. Wraps a baseline request context from which we reuse the
// networking components.
class DeviceManagementRequestContext : public net::URLRequestContext {
 public:
  explicit DeviceManagementRequestContext(net::URLRequestContext* base_context);
  virtual ~DeviceManagementRequestContext();

 private:
  // Overridden from net::URLRequestContext:
  virtual const std::string& GetUserAgent(const GURL& url) const;
};

DeviceManagementRequestContext::DeviceManagementRequestContext(
    net::URLRequestContext* base_context) {
  // Share resolver, proxy service and ssl bits with the baseline context. This
  // is important so we don't make redundant requests (e.g. when resolving proxy
  // auto configuration).
  net_log_ = base_context->net_log();
  host_resolver_ = base_context->host_resolver();
  proxy_service_ = base_context->proxy_service();
  ssl_config_service_ = base_context->ssl_config_service();

  // Share the http session.
  http_transaction_factory_ =
      new net::HttpNetworkLayer(
          base_context->http_transaction_factory()->GetSession());

  // No cookies, please.
  cookie_store_ = new net::CookieMonster(NULL, NULL);

  // Initialize these to sane values for our purposes.
  accept_language_ = "*";
  accept_charset_ = "*";
}

DeviceManagementRequestContext::~DeviceManagementRequestContext() {
  delete http_transaction_factory_;
  delete http_auth_handler_factory_;
}

const std::string& DeviceManagementRequestContext::GetUserAgent(
    const GURL& url) const {
  return webkit_glue::GetUserAgent(url);
}

// Request context holder.
class DeviceManagementRequestContextGetter : public URLRequestContextGetter {
 public:
  DeviceManagementRequestContextGetter(
      URLRequestContextGetter* base_context_getter)
      : base_context_getter_(base_context_getter) {}

  // Overridden from URLRequestContextGetter:
  virtual net::URLRequestContext* GetURLRequestContext();
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const;

 private:
  scoped_refptr<net::URLRequestContext> context_;
  scoped_refptr<URLRequestContextGetter> base_context_getter_;
};


net::URLRequestContext*
DeviceManagementRequestContextGetter::GetURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!context_) {
    context_ = new DeviceManagementRequestContext(
        base_context_getter_->GetURLRequestContext());
  }

  return context_.get();
}

scoped_refptr<base::MessageLoopProxy>
DeviceManagementRequestContextGetter::GetIOMessageLoopProxy() const {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
}

}  // namespace

DeviceManagementService::~DeviceManagementService() {
  // All running jobs should have been canceled by now. If not, there are
  // backend objects still around, which is an error.
  DCHECK(pending_jobs_.empty());
  DCHECK(queued_jobs_.empty());
}

DeviceManagementBackend* DeviceManagementService::CreateBackend() {
  return new DeviceManagementBackendImpl(this);
}

void DeviceManagementService::Initialize(
    URLRequestContextGetter* request_context_getter) {
  DCHECK(!request_context_getter_);
  request_context_getter_ =
      new DeviceManagementRequestContextGetter(request_context_getter);
  while (!queued_jobs_.empty()) {
    StartJob(queued_jobs_.front());
    queued_jobs_.pop_front();
  }
}

void DeviceManagementService::Shutdown() {
  for (JobFetcherMap::iterator job(pending_jobs_.begin());
       job != pending_jobs_.end();
       ++job) {
    delete job->first;
    queued_jobs_.push_back(job->second);
  }
}

DeviceManagementService::DeviceManagementService(
    const std::string& server_url)
    : server_url_(server_url) {
}

void DeviceManagementService::AddJob(DeviceManagementJob* job) {
  if (request_context_getter_.get())
    StartJob(job);
  else
    queued_jobs_.push_back(job);
}

void DeviceManagementService::RemoveJob(DeviceManagementJob* job) {
  for (JobFetcherMap::iterator entry(pending_jobs_.begin());
       entry != pending_jobs_.end();
       ++entry) {
    if (entry->second == job) {
      delete entry->first;
      pending_jobs_.erase(entry);
      break;
    }
  }
}

void DeviceManagementService::StartJob(DeviceManagementJob* job) {
  URLFetcher* fetcher = URLFetcher::Create(0, job->GetURL(server_url_),
                                           URLFetcher::POST, this);
  fetcher->set_load_flags(net::LOAD_DO_NOT_SEND_COOKIES |
                          net::LOAD_DO_NOT_SAVE_COOKIES |
                          net::LOAD_DISABLE_CACHE);
  fetcher->set_request_context(request_context_getter_.get());
  job->ConfigureRequest(fetcher);
  pending_jobs_[fetcher] = job;
  fetcher->Start();
}

void DeviceManagementService::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  JobFetcherMap::iterator entry(pending_jobs_.find(source));
  if (entry != pending_jobs_.end()) {
    DeviceManagementJob* job = entry->second;
    job->HandleResponse(status, response_code, cookies, data);
    pending_jobs_.erase(entry);
  } else {
    NOTREACHED() << "Callback from foreign URL fetcher";
  }
  delete source;
}

}  // namespace policy
