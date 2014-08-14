// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_APPLICATION_LIB_WEAK_SERVICE_PROVIDER_H_
#define MOJO_PUBLIC_APPLICATION_LIB_WEAK_SERVICE_PROVIDER_H_

#include "mojo/public/interfaces/application/service_provider.mojom.h"

namespace mojo {
class ServiceProviderImpl;
namespace internal {
class ServiceConnectorBase;

// Implements a weak pointer to a ServiceProvider. Necessary as the lifetime of
// the ServiceProviderImpl is bound to that of its pipe, but code may continue
// to reference a remote service provider beyond the lifetime of said pipe.
// Calls to ConnectToService() are silently dropped when the pipe is closed.
class WeakServiceProvider : public ServiceProvider {
 public:
  WeakServiceProvider(ServiceProviderImpl* creator,
                      ServiceProvider* service_provider);
  virtual ~WeakServiceProvider();

  void Clear();

 private:
  // Overridden from ServiceProvider:
  virtual void ConnectToService(
      const String& service_name,
      ScopedMessagePipeHandle client_handle) MOJO_OVERRIDE;

  ServiceProviderImpl* creator_;
  ServiceProvider* service_provider_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(WeakServiceProvider);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_APPLICATION_LIB_WEAK_SERVICE_PROVIDER_H_
