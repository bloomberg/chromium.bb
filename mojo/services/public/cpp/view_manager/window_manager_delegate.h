// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_WINDOW_MANAGER_DELEGATE_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_WINDOW_MANAGER_DELEGATE_H_

#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/services/public/interfaces/input_events/input_events.mojom.h"

namespace mojo {

class View;

// A WindowManagerDelegate is provided by the application embedded at the
// service root view.
class WindowManagerDelegate {
 public:
  // Create an appropriate view to embed |url|.
  virtual void Embed(const String& url,
                     InterfaceRequest<ServiceProvider> service_provider) {}

  // Dispatch the supplied input event to the appropriate view (taking into
  // account focus, activation, modality, etc.).
  virtual void DispatchEvent(View* target, EventPtr event) = 0;

 protected:
  virtual ~WindowManagerDelegate() {}
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_WINDOW_MANAGER_DELEGATE_H_
