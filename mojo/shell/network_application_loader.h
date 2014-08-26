// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_NETWORK_APPLICATION_LOADER_H_
#define MOJO_SHELL_NETWORK_APPLICATION_LOADER_H_

#include <map>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/application_manager/application_loader.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/services/network/network_context.h"

namespace mojo {

class ApplicationImpl;
class NetworkService;

namespace shell {

// ApplicationLoader responsible for creating connections to the NetworkService.
class NetworkApplicationLoader : public ApplicationLoader,
                                 public ApplicationDelegate,
                                 public InterfaceFactory<NetworkService> {
 public:
  NetworkApplicationLoader();
  virtual ~NetworkApplicationLoader();

 private:
  // ApplicationLoader overrides:
  virtual void Load(ApplicationManager* manager,
                    const GURL& url,
                    scoped_refptr<LoadCallbacks> callbacks) OVERRIDE;
  virtual void OnApplicationError(ApplicationManager* manager,
                                  const GURL& url) OVERRIDE;

  // ApplicationDelegate overrides.
  virtual void Initialize(ApplicationImpl* app) OVERRIDE;
  virtual bool ConfigureIncomingConnection(
      ApplicationConnection* connection) OVERRIDE;

  // InterfaceFactory<NetworkService> overrides.
  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<NetworkService> request) OVERRIDE;

  base::ScopedPtrHashMap<uintptr_t, ApplicationImpl> apps_;
  scoped_ptr<NetworkContext> context_;

  DISALLOW_COPY_AND_ASSIGN(NetworkApplicationLoader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_NETWORK_APPLICATION_LOADER_H_
