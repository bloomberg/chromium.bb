// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_WAYLAND_H_
#define BASE_MESSAGE_PUMP_WAYLAND_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/message_pump_glib.h"

typedef struct _GMainContext GMainContext;
typedef struct _GPollFD GPollFD;
typedef struct _GSource GSource;

namespace ui {
union WaylandEvent;
}  // namespace ui

namespace base {

// The documentation for this class is in message_pump_glib.h
class BASE_EXPORT MessagePumpObserver {
 public:
   enum EventStatus {
     EVENT_CONTINUE,    // The event should be dispatched as normal.
     EVENT_HANDLED      // The event should not be processed any farther.
   };

  // This method is called before processing an Event. If the method returns
  // EVENT_HANDLED, it indicates the event has already been handled, so the
  // event is not processed any farther. If the method returns EVENT_CONTINUE,
  // the event dispatching proceeds as normal.
  virtual EventStatus WillProcessEvent(ui::WaylandEvent* event);

 protected:
  virtual ~MessagePumpObserver() {}
};

// The documentation for this class is in message_pump_glib.h
//
// The nested loop is exited by either posting a quit, or returning false
// from Dispatch.
class MessagePumpDispatcher {
 public:
  enum DispatchStatus {
    EVENT_IGNORED,    // The event was not processed.
    EVENT_PROCESSED,  // The event has been processed.
    EVENT_QUIT        // The event was processed and the message-loop should
                      // terminate.
  };

  // Dispatches the event. If true is returned processing continues as
  // normal. If false is returned, the nested loop exits immediately.
  virtual DispatchStatus Dispatch(ui::WaylandEvent* event) = 0;

 protected:
  virtual ~MessagePumpDispatcher() {}
};

class BASE_EXPORT MessagePumpWayland : public MessagePumpGlib {

 public:
  MessagePumpWayland();
  virtual ~MessagePumpWayland();

  // Overridden from MessagePumpGlib
  virtual bool RunOnce(GMainContext* context, bool block) OVERRIDE;
 private:

  // This is a GLib structure that we can add event sources to.
  GMainContext* context_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpWayland);
};

typedef MessagePumpWayland MessagePumpForUI;

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_WAYLAND_H_
