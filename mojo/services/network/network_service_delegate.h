// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_NETWORK_SERVICE_DELEGATE_H_
#define MOJO_SERVICES_NETWORK_NETWORK_SERVICE_DELEGATE_H_

#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/services/network/network_context.h"
#include "mojo/services/network/network_service_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_ptr.h"

class NetworkServiceDelegate
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<mojo::NetworkService> {
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

 private:
  scoped_ptr<mojo::NetworkContext> context_;
  mojo::AppLifetimeHelper app_lifetime_helper_;
};

#endif  // MOJO_SERVICES_NETWORK_NETWORK_SERVICE_DELEGATE_H_
