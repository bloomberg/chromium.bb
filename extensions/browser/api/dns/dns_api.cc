// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/dns/dns_api.h"

#include "base/bind.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/api/dns.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"

using content::BrowserThread;
using extensions::api::dns::ResolveCallbackResolveInfo;

namespace Resolve = extensions::api::dns::Resolve;

namespace extensions {

DnsResolveFunction::DnsResolveFunction() : binding_(this) {}

DnsResolveFunction::~DnsResolveFunction() {}

ExtensionFunction::ResponseAction DnsResolveFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<Resolve::Params> params(Resolve::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  network::mojom::ResolveHostClientPtr client_ptr;
  binding_.Bind(mojo::MakeRequest(&client_ptr));
  binding_.set_connection_error_handler(
      base::BindOnce(&DnsResolveFunction::OnComplete, base::Unretained(this),
                     net::ERR_FAILED, base::nullopt));
  // Yes, we are passing zero as the port. There are some interesting but not
  // presently relevant reasons why HostResolver asks for the port of the
  // hostname you'd like to resolve, even though it doesn't use that value in
  // determining its answer.
  net::HostPortPair host_port_pair(params->hostname, 0);
  content::BrowserContext::GetDefaultStoragePartition(browser_context())
      ->GetNetworkContext()
      ->ResolveHost(host_port_pair, nullptr, std::move(client_ptr));

  // Balanced in OnComplete().
  AddRef();
  return RespondLater();
}

void DnsResolveFunction::OnComplete(
    int result,
    const base::Optional<net::AddressList>& resolved_addresses) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  binding_.Close();

  ResolveCallbackResolveInfo resolve_info;
  resolve_info.result_code = result;
  if (result == net::OK) {
    DCHECK(resolved_addresses.has_value() && !resolved_addresses->empty());
    resolve_info.address = std::make_unique<std::string>(
        resolved_addresses->front().ToStringWithoutPort());
  }

  Respond(ArgumentList(Resolve::Results::Create(resolve_info)));

  Release();  // Added in Run().
}

}  // namespace extensions
