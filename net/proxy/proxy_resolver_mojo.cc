// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_mojo.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/url_type_converters.h"
#include "net/base/net_errors.h"
#include "net/dns/mojo_host_resolver_impl.h"
#include "net/proxy/mojo_proxy_resolver_factory.h"
#include "net/proxy/mojo_proxy_type_converters.h"
#include "net/proxy/proxy_info.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

namespace net {

class ProxyResolverMojo::Job : public interfaces::ProxyResolverRequestClient,
                               public mojo::ErrorHandler {
 public:
  Job(ProxyResolverMojo* resolver,
      const GURL& url,
      ProxyInfo* results,
      const net::CompletionCallback& callback);
  ~Job() override;

  // Cancels the job and prevents the callback from being run.
  void Cancel();

  // Returns the LoadState of this job.
  LoadState load_state() { return load_state_; }

 private:
  // Overridden from mojo::ErrorHandler:
  void OnConnectionError() override;

  // Overridden from interfaces::ProxyResolverRequestClient:
  void ReportResult(
      int32_t error,
      mojo::Array<interfaces::ProxyServerPtr> proxy_servers) override;
  void LoadStateChanged(int32_t load_state) override;

  ProxyResolverMojo* resolver_;
  const GURL url_;
  ProxyInfo* results_;
  net::CompletionCallback callback_;
  LoadState load_state_ = LOAD_STATE_RESOLVING_PROXY_FOR_URL;

  base::ThreadChecker thread_checker_;
  mojo::Binding<interfaces::ProxyResolverRequestClient> binding_;
};

ProxyResolverMojo::Job::Job(ProxyResolverMojo* resolver,
                            const GURL& url,
                            ProxyInfo* results,
                            const net::CompletionCallback& callback)
    : resolver_(resolver),
      url_(url),
      results_(results),
      callback_(callback),
      binding_(this) {
  binding_.set_error_handler(this);

  interfaces::ProxyResolverRequestClientPtr client_ptr;
  binding_.Bind(mojo::GetProxy(&client_ptr));
  resolver_->mojo_proxy_resolver_ptr_->GetProxyForUrl(mojo::String::From(url_),
                                                      client_ptr.Pass());
}

ProxyResolverMojo::Job::~Job() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!callback_.is_null())
    callback_.Run(ERR_PAC_SCRIPT_TERMINATED);
}

void ProxyResolverMojo::Job::Cancel() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback_.is_null());
  callback_.Reset();
}

void ProxyResolverMojo::Job::OnConnectionError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "ProxyResolverMojo::Job::OnConnectionError";
  resolver_->RemoveJob(this);
}

void ProxyResolverMojo::Job::ReportResult(
    int32_t error,
    mojo::Array<interfaces::ProxyServerPtr> proxy_servers) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "ProxyResolverMojo::Job::ReportResult: " << error;

  if (error == OK) {
    *results_ = proxy_servers.To<ProxyInfo>();
    DVLOG(1) << "Servers: " << results_->ToPacString();
  }

  callback_.Run(error);
  callback_.Reset();
  resolver_->RemoveJob(this);
}

void ProxyResolverMojo::Job::LoadStateChanged(int32_t load_state) {
  load_state_ = static_cast<LoadState>(load_state);
}

ProxyResolverMojo::ProxyResolverMojo(
    MojoProxyResolverFactory* mojo_proxy_resolver_factory,
    HostResolver* host_resolver)
    : ProxyResolver(true /* |expects_pac_bytes| */),
      mojo_proxy_resolver_factory_(mojo_proxy_resolver_factory),
      host_resolver_(host_resolver) {
}

ProxyResolverMojo::~ProxyResolverMojo() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // All pending requests should have been cancelled.
  DCHECK(pending_jobs_.empty());
  DCHECK(set_pac_script_callback_.IsCancelled());
}

void ProxyResolverMojo::CancelSetPacScript() {
  DCHECK(thread_checker_.CalledOnValidThread());
  set_pac_script_callback_.Cancel();
}

int ProxyResolverMojo::SetPacScript(
    const scoped_refptr<ProxyResolverScriptData>& pac_script,
    const net::CompletionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(set_pac_script_callback_.IsCancelled());
  DCHECK(!callback.is_null());
  if (pac_script->type() != ProxyResolverScriptData::TYPE_SCRIPT_CONTENTS ||
      pac_script->utf16().empty()) {
    return ERR_PAC_SCRIPT_FAILED;
  }

  DVLOG(1) << "ProxyResolverMojo::SetPacScript: " << pac_script->utf16();
  set_pac_script_callback_.Reset(
      base::Bind(&ProxyResolverMojo::OnSetPacScriptDone, base::Unretained(this),
                 pac_script, callback));

  if (!mojo_proxy_resolver_ptr_)
    SetUpServices();

  mojo_proxy_resolver_ptr_->SetPacScript(
      mojo::String::From(pac_script->utf16()),
      set_pac_script_callback_.callback());

  return ERR_IO_PENDING;
}

void ProxyResolverMojo::OnSetPacScriptDone(
    const scoped_refptr<ProxyResolverScriptData>& pac_script,
    const net::CompletionCallback& callback,
    int32_t result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!set_pac_script_callback_.IsCancelled());
  DVLOG(1) << "ProxyResolverMojo::OnSetPacScriptDone: " << result;

  callback.Run(result);
  set_pac_script_callback_.Cancel();
}

void ProxyResolverMojo::SetUpServices() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // A Mojo service implementation must outlive its binding.
  mojo_host_resolver_binding_.reset();

  interfaces::HostResolverPtr mojo_host_resolver_ptr;
  mojo_host_resolver_.reset(new MojoHostResolverImpl(host_resolver_));
  mojo_host_resolver_binding_.reset(new mojo::Binding<interfaces::HostResolver>(
      mojo_host_resolver_.get(), mojo::GetProxy(&mojo_host_resolver_ptr)));
  mojo_proxy_resolver_ptr_.reset();
  mojo_proxy_resolver_factory_->Create(
      mojo::GetProxy(&mojo_proxy_resolver_ptr_), mojo_host_resolver_ptr.Pass());
  mojo_proxy_resolver_ptr_.set_error_handler(this);
}

void ProxyResolverMojo::AbortPendingRequests() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!set_pac_script_callback_.IsCancelled()) {
    set_pac_script_callback_.callback().Run(ERR_PAC_SCRIPT_TERMINATED);
    set_pac_script_callback_.Cancel();
  }

  // Need to use this loop because deleting a Job will cause its callback to be
  // run with a failure error code, which may cause other Jobs to be deleted.
  while (!pending_jobs_.empty()) {
    auto it = pending_jobs_.begin();
    Job* job = *it;
    pending_jobs_.erase(it);

    // Deleting the job will cause its completion callback to be run with an
    // ERR_PAC_SCRIPT_TERMINATED error.
    delete job;
  }
}

void ProxyResolverMojo::OnConnectionError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "ProxyResolverMojo::OnConnectionError";

  // Disconnect from the Mojo proxy resolver service. An attempt to reconnect
  // will happen on the next |SetPacScript()| request.
  mojo_proxy_resolver_ptr_.reset();

  // Aborting requests will invoke their callbacks, which may call
  // |SetPacScript()| and re-create the connection. So disconnect from the Mojo
  // service (above) before aborting the pending requests.
  AbortPendingRequests();
}

void ProxyResolverMojo::RemoveJob(Job* job) {
  DCHECK(thread_checker_.CalledOnValidThread());
  size_t num_erased = pending_jobs_.erase(job);
  DCHECK(num_erased);
  delete job;
}

int ProxyResolverMojo::GetProxyForURL(const GURL& url,
                                      ProxyInfo* results,
                                      const net::CompletionCallback& callback,
                                      RequestHandle* request,
                                      const BoundNetLog& net_log) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If the Mojo service is not connected, fail. The Mojo service is connected
  // when the script is set, which must be done after construction and after a
  // previous request returns ERR_PAC_SCRIPT_TERMINATED due to the Mojo proxy
  // resolver process crashing.
  if (!mojo_proxy_resolver_ptr_) {
    DVLOG(1) << "ProxyResolverMojo::GetProxyForURL: Mojo not connected";
    return ERR_PAC_SCRIPT_TERMINATED;
  }

  Job* job = new Job(this, url, results, callback);
  bool inserted = pending_jobs_.insert(job).second;
  DCHECK(inserted);
  *request = job;

  return ERR_IO_PENDING;
}

void ProxyResolverMojo::CancelRequest(RequestHandle request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Job* job = static_cast<Job*>(request);
  DCHECK(job);
  job->Cancel();
  RemoveJob(job);
}

LoadState ProxyResolverMojo::GetLoadState(RequestHandle request) const {
  Job* job = static_cast<Job*>(request);
  CHECK_EQ(1u, pending_jobs_.count(job));
  return job->load_state();
}

}  // namespace net
