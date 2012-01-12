// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/dns/dns_api.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

using content::BrowserThread;

namespace extensions {

const char kAddressKey[] = "address";
const char kResultCodeKey[] = "resultCode";

// static
net::HostResolver* DNSResolveFunction::host_resolver_for_testing;

DNSResolveFunction::DNSResolveFunction()
    : response_(false),
      io_thread_(g_browser_process->io_thread()),
      capturing_bound_net_log_(new net::CapturingBoundNetLog(
          net::CapturingNetLog::kUnbounded)),
      request_handle_(new net::HostResolver::RequestHandle()),
      addresses_(new net::AddressList) {
}

DNSResolveFunction::~DNSResolveFunction() {
}

// static
void DNSResolveFunction::set_host_resolver_for_testing(
    net::HostResolver* host_resolver_for_testing_param) {
  host_resolver_for_testing = host_resolver_for_testing_param;
}

bool DNSResolveFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &hostname_));
  bool result = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DNSResolveFunction::WorkOnIOThread, this));
  DCHECK(result);
  return true;
}

void DNSResolveFunction::WorkOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::HostResolver* host_resolver = host_resolver_for_testing ?
      host_resolver_for_testing : io_thread_->globals()->host_resolver.get();
  DCHECK(host_resolver);

  // Yes, we are passing zero as the port. There are some interesting but not
  // presently relevant reasons why HostResolver asks for the port of the
  // hostname you'd like to resolve, even though it doesn't use that value in
  // determining its answer.
  net::HostPortPair host_port_pair(hostname_, 0);

  net::HostResolver::RequestInfo request_info(host_port_pair);
  int resolve_result = host_resolver->Resolve(
      request_info, addresses_.get(),
      base::Bind(&DNSResolveFunction::OnLookupFinished, this),
      request_handle_.get(), capturing_bound_net_log_->bound());

  // Balanced in OnLookupFinished.
  AddRef();

  if (resolve_result != net::ERR_IO_PENDING)
    OnLookupFinished(resolve_result);
}

void DNSResolveFunction::OnLookupFinished(int resolve_result) {
  DictionaryValue* api_result = new DictionaryValue();
  api_result->SetInteger(kResultCodeKey, resolve_result);
  if (resolve_result == net::OK) {
    const struct addrinfo* head = addresses_->head();
    DCHECK(head);
    api_result->SetString(kAddressKey, net::NetAddressToString(head));
  }
  result_.reset(api_result);
  response_ = true;

  bool post_task_result = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DNSResolveFunction::RespondOnUIThread, this));
  DCHECK(post_task_result);

  Release();  // Added in WorkOnIOThread().
}

void DNSResolveFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(response_);
}

}  // namespace extensions
