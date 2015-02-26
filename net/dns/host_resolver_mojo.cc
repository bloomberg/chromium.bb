// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_mojo.h"

#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/dns/mojo_host_type_converters.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

namespace net {

class HostResolverMojo::Job : public interfaces::HostResolverRequestClient,
                              public mojo::ErrorHandler {
 public:
  Job(AddressList* addresses,
      const CompletionCallback& callback,
      mojo::InterfaceRequest<interfaces::HostResolverRequestClient> request);

 private:
  // interfaces::HostResolverRequestClient override.
  void ReportResult(int32_t error,
                    interfaces::AddressListPtr address_list) override;

  // mojo::ErrorHandler override.
  void OnConnectionError() override;

  AddressList* addresses_;
  CompletionCallback callback_;
  mojo::Binding<interfaces::HostResolverRequestClient> binding_;
};

HostResolverMojo::HostResolverMojo(interfaces::HostResolverPtr resolver,
                                   const base::Closure& disconnect_callback)
    : resolver_(resolver.Pass()), disconnect_callback_(disconnect_callback) {
  if (!disconnect_callback_.is_null())
    resolver_.set_error_handler(this);
}

HostResolverMojo::~HostResolverMojo() = default;

int HostResolverMojo::Resolve(const RequestInfo& info,
                              RequestPriority priority,
                              AddressList* addresses,
                              const CompletionCallback& callback,
                              RequestHandle* request_handle,
                              const BoundNetLog& source_net_log) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Resolve " << info.host_port_pair().ToString();
  interfaces::HostResolverRequestClientPtr handle;
  *request_handle = new Job(addresses, callback, mojo::GetProxy(&handle));
  resolver_->Resolve(interfaces::HostResolverRequestInfo::From(info),
                     handle.Pass());
  return ERR_IO_PENDING;
}

int HostResolverMojo::ResolveFromCache(const RequestInfo& info,
                                       AddressList* addresses,
                                       const BoundNetLog& source_net_log) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "ResolveFromCache " << info.host_port_pair().ToString();
  // TODO(sammc): Support caching.
  return ERR_DNS_CACHE_MISS;
}

void HostResolverMojo::CancelRequest(RequestHandle req) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Deleting the Job closes the HostResolverRequestClient connection,
  // signalling cancellation of the request.
  delete static_cast<Job*>(req);
}

void HostResolverMojo::OnConnectionError() {
  DCHECK(!disconnect_callback_.is_null());
  disconnect_callback_.Run();
}

HostResolverMojo::Job::Job(
    AddressList* addresses,
    const CompletionCallback& callback,
    mojo::InterfaceRequest<interfaces::HostResolverRequestClient> request)
    : addresses_(addresses),
      callback_(callback),
      binding_(this, request.Pass()) {
  binding_.set_error_handler(this);
}

void HostResolverMojo::Job::ReportResult(
    int32_t error,
    interfaces::AddressListPtr address_list) {
  if (error == OK && address_list)
    *addresses_ = address_list->To<AddressList>();
  callback_.Run(error);
  delete this;
}

void HostResolverMojo::Job::OnConnectionError() {
  ReportResult(ERR_FAILED, interfaces::AddressListPtr());
}

}  // namespace net
