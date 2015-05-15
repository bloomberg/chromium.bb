// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_mojo.h"

#include <set>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/thread_checker.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/url_type_converters.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/dns/mojo_host_resolver_impl.h"
#include "net/interfaces/host_resolver_service.mojom.h"
#include "net/interfaces/proxy_resolver_service.mojom.h"
#include "net/proxy/mojo_proxy_resolver_factory.h"
#include "net/proxy/mojo_proxy_type_converters.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_resolver_error_observer.h"
#include "net/proxy/proxy_resolver_script_data.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

namespace net {
namespace {

class ErrorObserverHolder : public interfaces::ProxyResolverErrorObserver {
 public:
  ErrorObserverHolder(
      scoped_ptr<net::ProxyResolverErrorObserver> error_observer,
      mojo::InterfaceRequest<interfaces::ProxyResolverErrorObserver> request);
  ~ErrorObserverHolder() override;

  void OnPacScriptError(int32_t line_number,
                        const mojo::String& error) override;

 private:
  scoped_ptr<net::ProxyResolverErrorObserver> error_observer_;
  mojo::Binding<interfaces::ProxyResolverErrorObserver> binding_;

  DISALLOW_COPY_AND_ASSIGN(ErrorObserverHolder);
};

ErrorObserverHolder::ErrorObserverHolder(
    scoped_ptr<net::ProxyResolverErrorObserver> error_observer,
    mojo::InterfaceRequest<interfaces::ProxyResolverErrorObserver> request)
    : error_observer_(error_observer.Pass()), binding_(this, request.Pass()) {
}

ErrorObserverHolder::~ErrorObserverHolder() = default;

void ErrorObserverHolder::OnPacScriptError(int32_t line_number,
                                           const mojo::String& error) {
  DCHECK(error_observer_);
  error_observer_->OnPACScriptError(line_number, error.To<base::string16>());
}

// Implementation of ProxyResolver that connects to a Mojo service to evaluate
// PAC scripts. This implementation only knows about Mojo services, and
// therefore that service may live in or out of process.
//
// This implementation reports disconnections from the Mojo service (i.e. if the
// service is out-of-process and that process crashes) using the error code
// ERR_PAC_SCRIPT_TERMINATED.
class ProxyResolverMojo : public ProxyResolver, public mojo::ErrorHandler {
 public:
  // Constructs a ProxyResolverMojo that connects to a mojo proxy resolver
  // implementation using |resolver_ptr|. The implementation uses
  // |host_resolver| as the DNS resolver, using |host_resolver_binding| to
  // communicate with it. When deleted, the closure contained within
  // |on_delete_callback_runner| will be run.
  // TODO(amistry): Add NetLog.
  ProxyResolverMojo(
      interfaces::ProxyResolverPtr resolver_ptr,
      scoped_ptr<interfaces::HostResolver> host_resolver,
      scoped_ptr<mojo::Binding<interfaces::HostResolver>> host_resolver_binding,
      scoped_ptr<base::ScopedClosureRunner> on_delete_callback_runner,
      scoped_ptr<ErrorObserverHolder> error_observer);
  ~ProxyResolverMojo() override;

  // ProxyResolver implementation:
  int GetProxyForURL(const GURL& url,
                     ProxyInfo* results,
                     const net::CompletionCallback& callback,
                     RequestHandle* request,
                     const BoundNetLog& net_log) override;
  void CancelRequest(RequestHandle request) override;
  LoadState GetLoadState(RequestHandle request) const override;
  void CancelSetPacScript() override;
  int SetPacScript(const scoped_refptr<ProxyResolverScriptData>& pac_script,
                   const net::CompletionCallback& callback) override;

 private:
  class Job;

  // Overridden from mojo::ErrorHandler:
  void OnConnectionError() override;

  void RemoveJob(Job* job);

  // Connection to the Mojo proxy resolver.
  interfaces::ProxyResolverPtr mojo_proxy_resolver_ptr_;

  // Mojo host resolver service and binding.
  scoped_ptr<interfaces::HostResolver> mojo_host_resolver_;
  scoped_ptr<mojo::Binding<interfaces::HostResolver>>
      mojo_host_resolver_binding_;

  scoped_ptr<ErrorObserverHolder> error_observer_;

  std::set<Job*> pending_jobs_;

  base::ThreadChecker thread_checker_;

  scoped_ptr<base::ScopedClosureRunner> on_delete_callback_runner_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverMojo);
};

class ProxyResolverMojo::Job : public interfaces::ProxyResolverRequestClient,
                               public mojo::ErrorHandler {
 public:
  Job(ProxyResolverMojo* resolver,
      const GURL& url,
      ProxyInfo* results,
      const CompletionCallback& callback);
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
  CompletionCallback callback_;
  LoadState load_state_ = LOAD_STATE_RESOLVING_PROXY_FOR_URL;

  base::ThreadChecker thread_checker_;
  mojo::Binding<interfaces::ProxyResolverRequestClient> binding_;
};

ProxyResolverMojo::Job::Job(ProxyResolverMojo* resolver,
                            const GURL& url,
                            ProxyInfo* results,
                            const CompletionCallback& callback)
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

  CompletionCallback callback = callback_;
  callback_.Reset();
  resolver_->RemoveJob(this);
  callback.Run(error);
}

void ProxyResolverMojo::Job::LoadStateChanged(int32_t load_state) {
  load_state_ = static_cast<LoadState>(load_state);
}

ProxyResolverMojo::ProxyResolverMojo(
    interfaces::ProxyResolverPtr resolver_ptr,
    scoped_ptr<interfaces::HostResolver> host_resolver,
    scoped_ptr<mojo::Binding<interfaces::HostResolver>> host_resolver_binding,
    scoped_ptr<base::ScopedClosureRunner> on_delete_callback_runner,
    scoped_ptr<ErrorObserverHolder> error_observer)
    : ProxyResolver(true),
      mojo_proxy_resolver_ptr_(resolver_ptr.Pass()),
      mojo_host_resolver_(host_resolver.Pass()),
      mojo_host_resolver_binding_(host_resolver_binding.Pass()),
      error_observer_(error_observer.Pass()),
      on_delete_callback_runner_(on_delete_callback_runner.Pass()) {
  mojo_proxy_resolver_ptr_.set_error_handler(this);
}

ProxyResolverMojo::~ProxyResolverMojo() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // All pending requests should have been cancelled.
  DCHECK(pending_jobs_.empty());
}

void ProxyResolverMojo::CancelSetPacScript() {
  NOTREACHED();
}

int ProxyResolverMojo::SetPacScript(
    const scoped_refptr<ProxyResolverScriptData>& pac_script,
    const CompletionCallback& callback) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

void ProxyResolverMojo::OnConnectionError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "ProxyResolverMojo::OnConnectionError";

  // Disconnect from the Mojo proxy resolver service.
  mojo_proxy_resolver_ptr_.reset();
}

void ProxyResolverMojo::RemoveJob(Job* job) {
  DCHECK(thread_checker_.CalledOnValidThread());
  size_t num_erased = pending_jobs_.erase(job);
  DCHECK(num_erased);
  delete job;
}

int ProxyResolverMojo::GetProxyForURL(const GURL& url,
                                      ProxyInfo* results,
                                      const CompletionCallback& callback,
                                      RequestHandle* request,
                                      const BoundNetLog& net_log) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!mojo_proxy_resolver_ptr_)
    return ERR_PAC_SCRIPT_TERMINATED;

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

}  // namespace

class ProxyResolverFactoryMojo::Job
    : public interfaces::ProxyResolverFactoryRequestClient,
      public mojo::ErrorHandler,
      public ProxyResolverFactory::Request {
 public:
  Job(ProxyResolverFactoryMojo* factory,
      const scoped_refptr<ProxyResolverScriptData>& pac_script,
      scoped_ptr<ProxyResolver>* resolver,
      const CompletionCallback& callback)
      : factory_(factory),
        resolver_(resolver),
        callback_(callback),
        binding_(this),
        host_resolver_(new MojoHostResolverImpl(factory_->host_resolver_)),
        host_resolver_binding_(
            new mojo::Binding<interfaces::HostResolver>(host_resolver_.get())) {
    interfaces::HostResolverPtr host_resolver_ptr;
    interfaces::ProxyResolverFactoryRequestClientPtr client_ptr;
    interfaces::ProxyResolverErrorObserverPtr error_observer_ptr;
    binding_.Bind(mojo::GetProxy(&client_ptr));
    if (!factory_->error_observer_factory_.is_null()) {
      scoped_ptr<ProxyResolverErrorObserver> error_observer =
          factory_->error_observer_factory_.Run();
      if (error_observer) {
        error_observer_.reset(new ErrorObserverHolder(
            error_observer.Pass(), mojo::GetProxy(&error_observer_ptr)));
      }
    }
    host_resolver_binding_->Bind(mojo::GetProxy(&host_resolver_ptr));
    on_delete_callback_runner_ = factory_->mojo_proxy_factory_->CreateResolver(
        mojo::String::From(pac_script->utf16()), mojo::GetProxy(&resolver_ptr_),
        host_resolver_ptr.Pass(), error_observer_ptr.Pass(), client_ptr.Pass());
    resolver_ptr_.set_error_handler(this);
    binding_.set_error_handler(this);
  }

  void OnConnectionError() override {
    callback_.Run(ERR_PAC_SCRIPT_TERMINATED);
    on_delete_callback_runner_.reset();
  }

 private:
  void ReportResult(int32_t error) override {
    resolver_ptr_.set_error_handler(nullptr);
    binding_.set_error_handler(nullptr);
    if (error == OK) {
      resolver_->reset(new ProxyResolverMojo(
          resolver_ptr_.Pass(), host_resolver_.Pass(),
          host_resolver_binding_.Pass(), on_delete_callback_runner_.Pass(),
          error_observer_.Pass()));
    }
    on_delete_callback_runner_.reset();
    callback_.Run(error);
  }

  ProxyResolverFactoryMojo* const factory_;
  scoped_ptr<ProxyResolver>* resolver_;
  const CompletionCallback callback_;
  interfaces::ProxyResolverPtr resolver_ptr_;
  mojo::Binding<interfaces::ProxyResolverFactoryRequestClient> binding_;
  scoped_ptr<interfaces::HostResolver> host_resolver_;
  scoped_ptr<mojo::Binding<interfaces::HostResolver>> host_resolver_binding_;
  scoped_ptr<base::ScopedClosureRunner> on_delete_callback_runner_;
  scoped_ptr<ErrorObserverHolder> error_observer_;
};

ProxyResolverFactoryMojo::ProxyResolverFactoryMojo(
    MojoProxyResolverFactory* mojo_proxy_factory,
    HostResolver* host_resolver,
    const base::Callback<scoped_ptr<ProxyResolverErrorObserver>()>&
        error_observer_factory)
    : ProxyResolverFactory(true),
      mojo_proxy_factory_(mojo_proxy_factory),
      host_resolver_(host_resolver),
      error_observer_factory_(error_observer_factory) {
}

ProxyResolverFactoryMojo::~ProxyResolverFactoryMojo() = default;

int ProxyResolverFactoryMojo::CreateProxyResolver(
    const scoped_refptr<ProxyResolverScriptData>& pac_script,
    scoped_ptr<ProxyResolver>* resolver,
    const CompletionCallback& callback,
    scoped_ptr<ProxyResolverFactory::Request>* request) {
  DCHECK(resolver);
  DCHECK(request);
  if (pac_script->type() != ProxyResolverScriptData::TYPE_SCRIPT_CONTENTS ||
      pac_script->utf16().empty()) {
    return ERR_PAC_SCRIPT_FAILED;
  }
  request->reset(new Job(this, pac_script, resolver, callback));
  return ERR_IO_PENDING;
}

}  // namespace net
