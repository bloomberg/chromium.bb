// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_GLIB_X_DISPATCH_H
#define BASE_MESSAGE_PUMP_GLIB_X_DISPATCH_H

#include "base/message_pump.h"
#include "base/message_pump_glib.h"

typedef union _XEvent XEvent;

namespace base {

// The message pump used for TOUCH_UI on linux is MessagePumpGlibX, which can
// dispatch both GdkEvents* and XEvents* captured directly from X.
// MessagePumpForUI::Dispatcher provides the mechanism for dispatching
// GdkEvents. This class provides additional mechanism for dispatching XEvents.
class MessagePumpGlibXDispatcher : public MessagePumpForUI::Dispatcher {
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
  virtual DispatchStatus DispatchX(XEvent* xevent) = 0;
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_GLIB_X_DISPATCH_H
