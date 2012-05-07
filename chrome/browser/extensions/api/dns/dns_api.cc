// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/dns/dns_api.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/common/extensions/api/experimental.dns.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"

using content::BrowserThread;
using extensions::api::experimental_dns::ResolveCallbackResolveInfo;

namespace Resolve = extensions::api::experimental_dns::Resolve;

namespace extensions {

namespace {

// If null, then we'll use io_thread_ to obtain the real HostResolver. We use
// a plain pointer for to be consistent with the ownership model of the real
// one.
net::HostResolver* g_host_resolver_for_testing = NULL;

}  // namespace

DnsResolveFunction::DnsResolveFunction()
    : response_(false),
      io_thread_(g_browser_process->io_thread()),
      capturing_bound_net_log_(new net::CapturingBoundNetLog(
          net::CapturingNetLog::kUnbounded)),
      request_handle_(new net::HostResolver::RequestHandle()),
      addresses_(new net::AddressList) {
}

// static
void DnsResolveFunction::set_host_resolver_for_testing(
    net::HostResolver* host_resolver_for_testing_param) {
  g_host_resolver_for_testing = host_resolver_for_testing_param;
}

DnsResolveFunction::~DnsResolveFunction() {}

bool DnsResolveFunction::RunImpl() {
  scoped_ptr<Resolve::Params> params(Resolve::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  hostname_ = params->hostname;

  bool result = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DnsResolveFunction::WorkOnIOThread, this));
  DCHECK(result);
  return true;
}

void DnsResolveFunction::WorkOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::HostResolver* host_resolver = g_host_resolver_for_testing ?
      g_host_resolver_for_testing : io_thread_->globals()->host_resolver.get();
  DCHECK(host_resolver);

  // Yes, we are passing zero as the port. There are some interesting but not
  // presently relevant reasons why HostResolver asks for the port of the
  // hostname you'd like to resolve, even though it doesn't use that value in
  // determining its answer.
  net::HostPortPair host_port_pair(hostname_, 0);

  net::HostResolver::RequestInfo request_info(host_port_pair);
  int resolve_result = host_resolver->Resolve(
      request_info, addresses_.get(),
      base::Bind(&DnsResolveFunction::OnLookupFinished, this),
      request_handle_.get(), capturing_bound_net_log_->bound());

  // Balanced in OnLookupFinished.
  AddRef();

  if (resolve_result != net::ERR_IO_PENDING)
    OnLookupFinished(resolve_result);
}

void DnsResolveFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(response_);
}

void DnsResolveFunction::OnLookupFinished(int resolve_result) {
  scoped_ptr<ResolveCallbackResolveInfo> resolve_info(
      new ResolveCallbackResolveInfo());
  resolve_info->result_code = resolve_result;
  if (resolve_result == net::OK) {
    DCHECK(!addresses_->empty());
    resolve_info->address.reset(
        new std::string(addresses_->front().ToStringWithoutPort()));
  }
  result_.reset(Resolve::Result::Create(*resolve_info));
  response_ = true;

  bool post_task_result = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DnsResolveFunction::RespondOnUIThread, this));
  DCHECK(post_task_result);

  Release();  // Added in WorkOnIOThread().
}

}  // namespace extensions
