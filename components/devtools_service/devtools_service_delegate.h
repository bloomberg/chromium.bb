// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_SERVICE_DELEGATE_H_
#define COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_SERVICE_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/devtools_service/public/interfaces/devtools_service.mojom.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/interface_factory.h"

namespace devtools_service {

class DevToolsService;

class DevToolsServiceDelegate
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<DevToolsRegistry>,
      public mojo::InterfaceFactory<DevToolsCoordinator> {
 public:
  DevToolsServiceDelegate();
  ~DevToolsServiceDelegate() override;

 private:
  // mojo::ApplicationDelegate implementation.
  void Initialize(mojo::ApplicationImpl* app) override;
  bool AcceptConnection(
      mojo::ApplicationConnection* connection) override;
  void Quit() override;

  // mojo::InterfaceFactory<DevToolsRegistry> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<DevToolsRegistry> request) override;

  // mojo::InterfaceFactory<DevToolsCoordinator> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<DevToolsCoordinator> request) override;

  scoped_ptr<DevToolsService> service_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsServiceDelegate);
};

}  // namespace devtools_service

#endif  // COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_SERVICE_DELEGATE_H_
