// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WINDOW_MANAGER_WINDOW_MANAGER_DELEGATE_H_
#define COMPONENTS_WINDOW_MANAGER_WINDOW_MANAGER_DELEGATE_H_

#include "third_party/mojo/src/mojo/public/cpp/bindings/string.h"
#include "third_party/mojo/src/mojo/public/interfaces/application/service_provider.mojom.h"

namespace window_manager {

class WindowManagerDelegate {
 public:
  // See WindowManager::Embed() for details.
  virtual void Embed(const mojo::String& url,
                     mojo::InterfaceRequest<mojo::ServiceProvider> services,
                     mojo::ServiceProviderPtr exposed_services) = 0;

 protected:
  virtual ~WindowManagerDelegate() {}
};

}  // namespace mojo

#endif  // COMPONENTS_WINDOW_MANAGER_WINDOW_MANAGER_DELEGATE_H_
