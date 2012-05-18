// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_X_H
#define BASE_MESSAGE_PUMP_X_H

#include "base/memory/scoped_ptr.h"
#include "base/message_pump.h"
#include "base/message_pump_glib.h"
#include "base/message_pump_dispatcher.h"
#include "base/message_pump_observer.h"

#include <bitset>

#include <glib.h>

typedef struct _XDisplay Display;

namespace base {

// This class implements a message-pump for dispatching X events.
class BASE_EXPORT MessagePumpX : public MessagePumpGlib {
 public:
  MessagePumpX();

  // Returns default X Display.
  static Display* GetDefaultXDisplay();

  // Returns true if the system supports XINPUT2.
  static bool HasXInput2();

  // Sets the default dispatcher to process native events.
  static void SetDefaultDispatcher(MessagePumpDispatcher* dispatcher);

  // Internal function. Called by the glib source dispatch function. Processes
  // all available X events.
  gboolean DispatchXEvents();

 protected:
  virtual ~MessagePumpX();

 private:
  // Initializes the glib event source for X.
  void InitXSource();

  // Dispatches the XEvent and returns true if we should exit the current loop
  // of message processing.
  bool ProcessXEvent(MessagePumpDispatcher* dispatcher, XEvent* event);

  // Sends the event to the observers. If an observer returns true, then it does
  // not send the event to any other observers and returns true. Returns false
  // if no observer returns true.
  bool WillProcessXEvent(XEvent* xevent);
  void DidProcessXEvent(XEvent* xevent);

  // The event source for X events.
  GSource* x_source_;

  // The poll attached to |x_source_|.
  scoped_ptr<GPollFD> x_poll_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpX);
};

typedef MessagePumpX MessagePumpForUI;

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_X_H
