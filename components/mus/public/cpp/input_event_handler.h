// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_INPUT_EVENT_HANDLER_H_
#define COMPONENTS_MUS_PUBLIC_CPP_INPUT_EVENT_HANDLER_H_

#include "base/callback_forward.h"
#include "components/mus/public/interfaces/input_events.mojom.h"

namespace mus {

class Window;

// Responsible for processing input events for mus::Window.
class InputEventHandler {
 public:
  // The event handler can asynchronously ack the event by taking ownership of
  // the |ack_callback|. If the handler does not take ownership of the callback,
  // then WindowTreeClientImpl will ack the event.
  virtual void OnWindowInputEvent(Window* target,
                                  mojom::EventPtr event,
                                  scoped_ptr<base::Closure>* ack_callback) = 0;
 protected:
  virtual ~InputEventHandler() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_INPUT_EVENT_HANDLER_H_
