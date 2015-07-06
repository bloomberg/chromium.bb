// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_NETWORK_SERVICE_DELEGATE_H_
#define MOJO_SERVICES_NETWORK_NETWORK_SERVICE_DELEGATE_H_

#include "base/threading/thread.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/services/network/network_context.h"
#include "mojo/services/network/public/interfaces/network_service.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader_factory.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_ptr.h"

namespace sql {
class ScopedMojoFilesystemVFS;
}

class NetworkServiceDelegate
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<mojo::NetworkService>,
      public mojo::InterfaceFactory<mojo::URLLoaderFactory> {
 public:
  NetworkServiceDelegate();
  ~NetworkServiceDelegate() override;

 private:
  // mojo::ApplicationDelegate implementation.
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;
  void Quit() override;

  // mojo::InterfaceFactory<mojo::NetworkService> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::NetworkService> request) override;

  // mojo::InterfaceFactory<mojo::URLLoaderFactory> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::URLLoaderFactory> request) override;

 private:
  mojo::ApplicationImpl* app_;

  // A worker thread that blocks for file IO.
  scoped_ptr<base::Thread> io_worker_thread_;

  // Our connection to the filesystem service, which stores our cookies and
  // other data.
  filesystem::FileSystemPtr files_;

  scoped_ptr<mojo::NetworkContext> context_;
};

#endif  // MOJO_SERVICES_NETWORK_NETWORK_SERVICE_DELEGATE_H_
