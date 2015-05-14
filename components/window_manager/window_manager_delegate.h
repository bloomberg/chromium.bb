// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WINDOW_MANAGER_WINDOW_MANAGER_DELEGATE_H_
#define COMPONENTS_WINDOW_MANAGER_WINDOW_MANAGER_DELEGATE_H_

#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/string.h"
#include "ui/mojo/events/input_events.mojom.h"
#include "ui/mojo/events/input_key_codes.mojom.h"

namespace window_manager {

class WindowManagerDelegate {
 public:
  // See WindowManager::Embed() for details.
  virtual void Embed(const mojo::String& url,
                     mojo::InterfaceRequest<mojo::ServiceProvider> services,
                     mojo::ServiceProviderPtr exposed_services) = 0;

  virtual void OnAcceleratorPressed(mojo::View* view,
                                    mojo::KeyboardCode keyboard_code,
                                    mojo::EventFlags flags) {}

 protected:
  virtual ~WindowManagerDelegate() {}
};

}  // namespace mojo

#endif  // COMPONENTS_WINDOW_MANAGER_WINDOW_MANAGER_DELEGATE_H_
