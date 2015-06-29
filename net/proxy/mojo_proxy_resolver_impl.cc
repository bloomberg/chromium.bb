// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/mojo_proxy_resolver_impl.h"

#include "base/stl_util.h"
#include "mojo/common/url_type_converters.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/proxy/mojo_proxy_type_converters.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_resolver_script_data.h"

namespace net {

class MojoProxyResolverImpl::Job : public mojo::ErrorHandler {
 public:
  Job(interfaces::ProxyResolverRequestClientPtr client,
      MojoProxyResolverImpl* resolver,
      const GURL& url);
  ~Job() override;

  void Start();

  net::ProxyResolver::RequestHandle request_handle() { return request_handle_; }

 private:
  // mojo::ErrorHandler override.
  // This is invoked in response to the client disconnecting, indicating
  // cancellation.
  void OnConnectionError() override;

  void GetProxyDone(int error);

  MojoProxyResolverImpl* resolver_;

  interfaces::ProxyResolverRequestClientPtr client_;
  ProxyInfo result_;
  GURL url_;
  net::ProxyResolver::RequestHandle request_handle_;
  bool done_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

MojoProxyResolverImpl::MojoProxyResolverImpl(
    scoped_ptr<net::ProxyResolver> resolver)
    : resolver_(resolver.Pass()) {
}

MojoProxyResolverImpl::~MojoProxyResolverImpl() {
  STLDeleteElements(&resolve_jobs_);
}

void MojoProxyResolverImpl::GetProxyForUrl(
    const mojo::String& url,
    interfaces::ProxyResolverRequestClientPtr client) {
  DVLOG(1) << "GetProxyForUrl(" << url << ")";
  Job* job = new Job(client.Pass(), this, url.To<GURL>());
  bool inserted = resolve_jobs_.insert(job).second;
  DCHECK(inserted);
  job->Start();
}

void MojoProxyResolverImpl::DeleteJob(Job* job) {
  if (job->request_handle())
    request_handle_to_job_.erase(job->request_handle());

  size_t num_erased = resolve_jobs_.erase(job);
  DCHECK(num_erased);
  delete job;
}

MojoProxyResolverImpl::Job::Job(
    interfaces::ProxyResolverRequestClientPtr client,
    MojoProxyResolverImpl* resolver,
    const GURL& url)
    : resolver_(resolver),
      client_(client.Pass()),
      url_(url),
      request_handle_(nullptr),
      done_(false) {
}

MojoProxyResolverImpl::Job::~Job() {
  if (request_handle_ && !done_)
    resolver_->resolver_->CancelRequest(request_handle_);
}

void MojoProxyResolverImpl::Job::Start() {
  int result = resolver_->resolver_->GetProxyForURL(
      url_, &result_, base::Bind(&Job::GetProxyDone, base::Unretained(this)),
      &request_handle_, BoundNetLog());
  if (result != ERR_IO_PENDING) {
    GetProxyDone(result);
    return;
  }
  client_.set_error_handler(this);
  resolver_->request_handle_to_job_.insert(
      std::make_pair(request_handle_, this));
}

void MojoProxyResolverImpl::Job::GetProxyDone(int error) {
  done_ = true;
  DVLOG(1) << "GetProxyForUrl(" << url_ << ") finished with error " << error
           << ". " << result_.proxy_list().size() << " Proxies returned:";
  for (const auto& proxy : result_.proxy_list().GetAll()) {
    DVLOG(1) << proxy.ToURI();
  }
  mojo::Array<interfaces::ProxyServerPtr> result;
  if (error == OK) {
    result = mojo::Array<interfaces::ProxyServerPtr>::From(
        result_.proxy_list().GetAll());
  }
  client_->ReportResult(error, result.Pass());
  resolver_->DeleteJob(this);
}

void MojoProxyResolverImpl::Job::OnConnectionError() {
  resolver_->DeleteJob(this);
}

}  // namespace net
