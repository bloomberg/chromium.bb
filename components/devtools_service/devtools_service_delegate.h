// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_SERVICE_DELEGATE_H_
#define COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_SERVICE_DELEGATE_H_

#include "base/macros.h"
#include "components/devtools_service/public/interfaces/devtools_service.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"

namespace devtools_service {

class DevToolsServiceDelegate : public mojo::ApplicationDelegate {
 public:
  DevToolsServiceDelegate();
  ~DevToolsServiceDelegate() override;

 private:
  // ApplicationDelegate implementation.
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;
  void Quit() override;

  DISALLOW_COPY_AND_ASSIGN(DevToolsServiceDelegate);
};

}  // namespace devtools_service

#endif  // COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_SERVICE_DELEGATE_H_
