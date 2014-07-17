// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_NETWORK_SERVICE_LOADER_H_
#define MOJO_SHELL_NETWORK_SERVICE_LOADER_H_

#include <map>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/service_manager/service_loader.h"
#include "mojo/services/network/network_context.h"

namespace mojo {

class ApplicationImpl;

namespace shell {

// ServiceLoader responsible for creating connections to the NetworkService.
class NetworkServiceLoader : public ServiceLoader, public ApplicationDelegate {
 public:
  NetworkServiceLoader();
  virtual ~NetworkServiceLoader();

 private:
  // ServiceLoader overrides:
  virtual void LoadService(
      ServiceManager* manager,
      const GURL& url,
      ScopedMessagePipeHandle shell_handle) OVERRIDE;
  virtual void OnServiceError(ServiceManager* manager,
                              const GURL& url) OVERRIDE;

  // ApplicationDelegate overrides.
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE;
  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE;

  base::ScopedPtrHashMap<uintptr_t, ApplicationImpl> apps_;
  scoped_ptr<NetworkContext> context_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceLoader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_NETWORK_SERVICE_LOADER_H_
