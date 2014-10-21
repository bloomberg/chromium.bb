// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_WINDOW_MANAGER_WNDOW_MANAGER_SERVICE_IMPL_H_
#define MOJO_SERVICES_WINDOW_MANAGER_WNDOW_MANAGER_SERVICE_IMPL_H_

#include "base/basictypes.h"
#include "mojo/services/public/interfaces/window_manager/window_manager.mojom.h"

namespace mojo {

class WindowManagerApp;

class WindowManagerServiceImpl : public InterfaceImpl<WindowManagerService> {
 public:
  explicit WindowManagerServiceImpl(WindowManagerApp* app);
  ~WindowManagerServiceImpl() override;

 private:
  // WindowManagerServiceImpl:
  void Embed(const String& url,
             InterfaceRequest<ServiceProvider> service_provider) override;
  void OnViewInputEvent(mojo::EventPtr event) override;

  // InterfaceImpl:
  void OnConnectionEstablished() override;

  WindowManagerApp* app_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerServiceImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_WINDOW_MANAGER_WNDOW_MANAGER_SERVICE_IMPL_H_
