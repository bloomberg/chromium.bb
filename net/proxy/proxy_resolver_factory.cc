// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_factory.h"

#include "net/base/net_errors.h"
#include "net/proxy/proxy_resolver.h"

namespace net {

class LegacyProxyResolverFactory::Job : public ProxyResolverFactory::Request {
 public:
  Job(LegacyProxyResolverFactory* factory,
      const scoped_refptr<ProxyResolverScriptData>& pac_script,
      scoped_ptr<ProxyResolver> resolver,
      scoped_ptr<ProxyResolver>* resolver_out,
      const net::CompletionCallback& callback);
  ~Job() override;
  int Start();

  void FactoryDestroyed();

 private:
  void OnSetPacScriptDone(int error);

  LegacyProxyResolverFactory* factory_;
  const scoped_refptr<ProxyResolverScriptData> pac_script_;
  scoped_ptr<ProxyResolver> resolver_;
  scoped_ptr<ProxyResolver>* resolver_out_;
  const net::CompletionCallback callback_;
};

LegacyProxyResolverFactory::Job::Job(
    LegacyProxyResolverFactory* factory,
    const scoped_refptr<ProxyResolverScriptData>& pac_script,
    scoped_ptr<ProxyResolver> resolver,
    scoped_ptr<ProxyResolver>* resolver_out,
    const net::CompletionCallback& callback)
    : factory_(factory),
      pac_script_(pac_script),
      resolver_(resolver.Pass()),
      resolver_out_(resolver_out),
      callback_(callback) {
}

LegacyProxyResolverFactory::Job::~Job() {
  if (factory_) {
    resolver_->CancelSetPacScript();
    factory_->RemoveJob(this);
  }
}

int LegacyProxyResolverFactory::Job::Start() {
  int error = resolver_->SetPacScript(
      pac_script_,
      base::Bind(&Job::OnSetPacScriptDone, base::Unretained(this)));
  if (error != ERR_IO_PENDING) {
    factory_ = nullptr;
    if (error == OK)
      *resolver_out_ = resolver_.Pass();
    return error;
  }

  return ERR_IO_PENDING;
}

void LegacyProxyResolverFactory::Job::FactoryDestroyed() {
  factory_ = nullptr;
  resolver_->CancelSetPacScript();
}

void LegacyProxyResolverFactory::Job::OnSetPacScriptDone(int error) {
  factory_->RemoveJob(this);
  if (error == OK)
    *resolver_out_ = resolver_.Pass();

  CompletionCallback callback = callback_;
  factory_ = nullptr;
  if (!callback.is_null())
    callback.Run(error);
}

ProxyResolverFactory::ProxyResolverFactory(bool expects_pac_bytes)
    : expects_pac_bytes_(expects_pac_bytes) {
}

ProxyResolverFactory::~ProxyResolverFactory() {
}

LegacyProxyResolverFactory::LegacyProxyResolverFactory(bool expects_pac_bytes)
    : ProxyResolverFactory(expects_pac_bytes) {
}

LegacyProxyResolverFactory::~LegacyProxyResolverFactory() {
  for (auto job : jobs_) {
    job->FactoryDestroyed();
  }
}

int LegacyProxyResolverFactory::CreateProxyResolver(
    const scoped_refptr<ProxyResolverScriptData>& pac_script,
    scoped_ptr<ProxyResolver>* resolver,
    const net::CompletionCallback& callback,
    scoped_ptr<ProxyResolverFactory::Request>* request) {
  scoped_ptr<Job> job(
      new Job(this, pac_script, CreateProxyResolver(), resolver, callback));
  int error = job->Start();
  if (error != ERR_IO_PENDING)
    return error;

  jobs_.insert(job.get());
  *request = job.Pass();
  return ERR_IO_PENDING;
}

void LegacyProxyResolverFactory::RemoveJob(Job* job) {
  size_t erased = jobs_.erase(job);
  DCHECK_EQ(1u, erased);
}

}  // namespace net
