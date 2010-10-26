// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
  // Dispatches the event. If true is returned processing continues as
  // normal. If false is returned, the nested loop exits immediately.
  virtual bool Dispatch(XEvent* xevent) = 0;
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_GLIB_X_DISPATCH_H
