// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_WINDOW_MANAGER_DELEGATE_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_WINDOW_MANAGER_DELEGATE_H_

#include "mojo/services/public/interfaces/input_events/input_events.mojom.h"

namespace mojo {
namespace view_manager {

class View;

// A WindowManagerDelegate is provided by the application embedded at the
// service root node.
class WindowManagerDelegate {
 public:
  // Create an appropriate node to embed |url|.
  virtual void EmbedRoot(const String& url) = 0;

  // Dispatch the supplied input event to the appropriate view (taking into
  // account focus, activation, modality, etc.).
  virtual void DispatchEvent(View* target, EventPtr event) = 0;

 protected:
  virtual ~WindowManagerDelegate() {}
};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_WINDOW_MANAGER_DELEGATE_H_
