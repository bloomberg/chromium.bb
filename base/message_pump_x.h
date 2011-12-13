// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_X_H
#define BASE_MESSAGE_PUMP_X_H

#include "base/message_pump.h"
#include "base/message_pump_glib.h"
#include "base/message_pump_observer.h"

#include <bitset>

#include <glib.h>

typedef struct _XDisplay Display;

namespace base {

// The documentation for this class is in message_pump_glib.h
//
// The nested loop is exited by either posting a quit, or returning EVENT_QUIT
// from Dispatch.
class MessagePumpDispatcher {
 public:
  enum DispatchStatus {
    EVENT_IGNORED,    // The event was not processed.
    EVENT_PROCESSED,  // The event has been processed.
    EVENT_QUIT        // The event was processed and the message-loop should
                      // terminate.
  };

  // Dispatches the event. EVENT_IGNORED is returned if the event was ignored
  // (i.e. not processed). EVENT_PROCESSED is returned if the event was
  // processed. The nested loop exits immediately if EVENT_QUIT is returned.
  virtual DispatchStatus Dispatch(XEvent* xevent) = 0;

 protected:
  virtual ~MessagePumpDispatcher() {}
};

// This class implements a message-pump for dispatching X events.
class BASE_EXPORT MessagePumpX : public MessagePumpGlib {
 public:
  MessagePumpX();
  virtual ~MessagePumpX();

  // Overridden from MessagePumpGlib:
  virtual bool RunOnce(GMainContext* context, bool block) OVERRIDE;

  // Returns default X Display.
  static Display* GetDefaultXDisplay();

  // Returns true if the system supports XINPUT2.
  static bool HasXInput2();

  // Sets the default dispatcher to process native events.
  static void SetDefaultDispatcher(MessagePumpDispatcher* dispatcher);

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

  DISALLOW_COPY_AND_ASSIGN(MessagePumpX);
};

typedef MessagePumpX MessagePumpForUI;

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_X_H
