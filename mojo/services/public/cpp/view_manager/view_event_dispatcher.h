// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_EVENT_DISPATCHER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_EVENT_DISPATCHER_H_

#include "mojo/services/public/interfaces/input_events/input_events.mojom.h"

namespace mojo {
namespace view_manager {

class View;

// A ViewEventDispatcher is provided by the application rendering at the root
// of a Node hierarchy. It is responsible for targeting input events to the
// relevant Views. This allows window manager features like focus, activation,
// modality, etc. to be implemented.
class ViewEventDispatcher {
 public:
  virtual void DispatchEvent(View* target, EventPtr event) = 0;

 protected:
  virtual ~ViewEventDispatcher() {}
};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_EVENT_DISPATCHER_H_
