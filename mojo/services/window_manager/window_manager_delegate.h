// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_DELEGATE_H_
#define MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_DELEGATE_H_

#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"

namespace mojo {

class WindowManagerDelegate {
 public:
  // See WindowManager::Embed() for details.
  virtual void Embed(const String& url,
                     InterfaceRequest<ServiceProvider> service_provider) = 0;

 protected:
  virtual ~WindowManagerDelegate() {}
};

}  // namespace mojo

#endif  // MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_DELEGATE_H_
