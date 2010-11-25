// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Funnel of Chrome Extension Window Events.

#ifndef CEEE_IE_BROKER_WINDOW_EVENTS_FUNNEL_H_
#define CEEE_IE_BROKER_WINDOW_EVENTS_FUNNEL_H_

#include <wtypes.h>  // For HRESULT and others...

#include "base/basictypes.h"


// Fwd.
class Value;


// Implements a set of methods to send window related events to the Chrome.
class WindowEventsFunnel {
 public:
  WindowEventsFunnel() {}
  virtual ~WindowEventsFunnel() {}

  // Un/Register our window to receive Shell Hook Notification Messages.
  static void Initialize();
  static void Terminate();

  // Sends the windows.onCreated event to Chrome.
  // @param window The HWND of the window that was created.
  virtual HRESULT OnCreated(HWND window);

  // Sends the windows.onFocusChanged event to Chrome.
  // @param window_id The identifier of the window that received the focus.
  virtual HRESULT OnFocusChanged(int window_id);

  // Sends the windows.onRemoved event to Chrome.
  // @param window The HWND of the window that was removed.
  virtual HRESULT OnRemoved(HWND window);

 protected:
  // Send the given event to Chrome.
  // @param event_name The name of the event.
  // @param event_args The arguments to be sent with the event.
  // protected virtual for testability...
  virtual HRESULT SendEvent(const char* event_name,
                            const Value& event_args);
 private:
  DISALLOW_COPY_AND_ASSIGN(WindowEventsFunnel);
};

#endif  // CEEE_IE_BROKER_WINDOW_EVENTS_FUNNEL_H_
