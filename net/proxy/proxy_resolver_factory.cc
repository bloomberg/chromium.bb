// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_factory.h"

#include "net/base/net_errors.h"
#include "net/proxy/proxy_resolver.h"

namespace net {
namespace {

class Job : public ProxyResolverFactory::Request {
 public:
  Job(const scoped_refptr<ProxyResolverScriptData>& pac_script,
      scoped_ptr<ProxyResolver> resolver,
      scoped_ptr<ProxyResolver>* resolver_out,
      const net::CompletionCallback& callback);
  ~Job() override;
  int Start();

 private:
  void OnSetPacScriptDone(int error);

  const scoped_refptr<ProxyResolverScriptData> pac_script_;
  scoped_ptr<ProxyResolver> resolver_;
  scoped_ptr<ProxyResolver>* resolver_out_;
  const net::CompletionCallback callback_;
  bool in_progress_;
};

Job::Job(const scoped_refptr<ProxyResolverScriptData>& pac_script,
         scoped_ptr<ProxyResolver> resolver,
         scoped_ptr<ProxyResolver>* resolver_out,
         const net::CompletionCallback& callback)
    : pac_script_(pac_script),
      resolver_(resolver.Pass()),
      resolver_out_(resolver_out),
      callback_(callback),
      in_progress_(false) {
}

Job::~Job() {
  if (in_progress_)
    resolver_->CancelSetPacScript();
}

int Job::Start() {
  int error = resolver_->SetPacScript(
      pac_script_,
      base::Bind(&Job::OnSetPacScriptDone, base::Unretained(this)));
  if (error != ERR_IO_PENDING) {
    if (error == OK)
      *resolver_out_ = resolver_.Pass();
    return error;
  }

  in_progress_ = true;
  return ERR_IO_PENDING;
}

void Job::OnSetPacScriptDone(int error) {
  if (error == OK)
    *resolver_out_ = resolver_.Pass();

  CompletionCallback callback = callback_;
  in_progress_ = false;
  if (!callback.is_null())
    callback.Run(error);
}

}  // namespace

ProxyResolverFactory::ProxyResolverFactory(bool expects_pac_bytes)
    : expects_pac_bytes_(expects_pac_bytes) {
}

ProxyResolverFactory::~ProxyResolverFactory() {
}

LegacyProxyResolverFactory::LegacyProxyResolverFactory(bool expects_pac_bytes)
    : ProxyResolverFactory(expects_pac_bytes) {
}

LegacyProxyResolverFactory::~LegacyProxyResolverFactory() {
}

int LegacyProxyResolverFactory::CreateProxyResolver(
    const scoped_refptr<ProxyResolverScriptData>& pac_script,
    scoped_ptr<ProxyResolver>* resolver,
    const net::CompletionCallback& callback,
    scoped_ptr<ProxyResolverFactory::Request>* request) {
  scoped_ptr<Job> job(
      new Job(pac_script, CreateProxyResolver(), resolver, callback));
  int error = job->Start();
  if (error != ERR_IO_PENDING)
    return error;

  *request = job.Pass();
  return ERR_IO_PENDING;
}

}  // namespace net
