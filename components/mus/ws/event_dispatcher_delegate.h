// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_EVENT_DISPATCHER_DELEGATE_H_
#define COMPONENTS_MUS_WS_EVENT_DISPATCHER_DELEGATE_H_

#include "ui/mojo/events/input_events.mojom.h"

namespace mus {
namespace ws {

class ServerWindow;

// Used by EventDispatcher for mocking in tests.
class EventDispatcherDelegate {
 public:
  virtual void OnAccelerator(uint32_t accelerator, mojo::EventPtr event) = 0;

  virtual void SetFocusedWindowFromEventDispatcher(ServerWindow* window) = 0;
  virtual ServerWindow* GetFocusedWindowForEventDispatcher() = 0;

  virtual void DispatchInputEventToWindow(ServerWindow* target,
                                          mojo::EventPtr event) = 0;

 protected:
  virtual ~EventDispatcherDelegate() {}
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_EVENT_DISPATCHER_DELEGATE_H_
