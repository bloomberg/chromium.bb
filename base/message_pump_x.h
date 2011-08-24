// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_X_H
#define BASE_MESSAGE_PUMP_X_H

#include "base/message_pump.h"
#include "base/message_pump_glib.h"

#include <bitset>

#include <glib.h>
#include <gtk/gtk.h>

typedef union _XEvent XEvent;
typedef struct _XDisplay Display;

namespace base {

// The documentation for this class is in message_pump_glib.h
class BASE_EXPORT MessagePumpObserver {
 public:
   enum EventStatus {
     EVENT_CONTINUE,    // The event should be dispatched as normal.
     EVENT_HANDLED      // The event should not be processed any farther.
   };

  // This method is called before processing an XEvent. If the method returns
  // EVENT_HANDLED, it indicates the event has already been handled, so the
  // event is not processed any farther. If the method returns EVENT_CONTINUE,
  // the event dispatching proceeds as normal.
  virtual EventStatus WillProcessXEvent(XEvent* xevent);

 protected:
  virtual ~MessagePumpObserver() {}
};

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

  // Indicates whether a GDK event was injected by chrome (when |true|) or if it
  // was captured and being processed by GDK (when |false|).
  bool IsDispatchingEvent(void) { return dispatching_event_; }

  // Overridden from MessagePumpGlib:
  virtual bool RunOnce(GMainContext* context, bool block) OVERRIDE;

  // Disables Gtk/Gdk event pumping. This will be used when
  // NativeWidgetX is enabled.
  static void DisableGtkMessagePump();

  // Returns default X Display.
  static Display* GetDefaultXDisplay();

  // Returns true if the system supports XINPUT2.
  static bool HasXInput2();

 private:
  // Some XEvent's can't be directly read from X event queue and will go
  // through GDK's dispatching process and may get discarded. This function
  // sets up a filter to intercept those XEvent's we are interested in
  // and dispatches them so that they won't get lost.
  static GdkFilterReturn GdkEventFilter(GdkXEvent* gxevent,
                                        GdkEvent* gevent,
                                        gpointer data);

  static void EventDispatcherX(GdkEvent* event, gpointer data);

  // Decides whether we are interested in processing this XEvent.
  bool ShouldCaptureXEvent(XEvent* event);

  // Dispatches the XEvent and returns true if we should exit the current loop
  // of message processing.
  bool ProcessXEvent(XEvent* event);

  // Sends the event to the observers. If an observer returns true, then it does
  // not send the event to any other observers and returns true. Returns false
  // if no observer returns true.
  bool WillProcessXEvent(XEvent* xevent);

  // Update the lookup table and flag the events that should be captured and
  // processed so that GDK doesn't get to them.
  void InitializeEventsToCapture(void);

  // The event source for X events (used only when GTK event processing is
  // disabled).
  GSource* x_source_;

  // The event source for GDK events.
  GSource* gdksource_;

  // The default GDK event dispatcher. This is stored so that it can be restored
  // when necessary during nested event dispatching.
  gboolean (*gdkdispatcher_)(GSource*, GSourceFunc, void*);

  // Indicates whether a GDK event was injected by chrome (when |true|) or if it
  // was captured and being processed by GDK (when |false|).
  bool dispatching_event_;

#if ! GTK_CHECK_VERSION(2,18,0)
// GDK_EVENT_LAST was introduced in GTK+ 2.18.0. For earlier versions, we pick a
// large enough value (the value of GDK_EVENT_LAST in 2.18.0) so that it works
// for all versions.
#define GDK_EVENT_LAST 37
#endif

// Ideally we would #include X.h for LASTEvent, but it brings in a lot of stupid
// stuff (like Time, CurrentTime etc.) that messes up a lot of things. So define
// XLASTEvent here to a large enough value so that it works.
#define XLASTEvent 40

  // We do not want to process all the events ourselves. So we use a lookup
  // table to quickly check if a particular event should be handled by us or if
  // it should be passed on to the default GDK handler.
  std::bitset<XLASTEvent> capture_x_events_;
  std::bitset<GDK_EVENT_LAST> capture_gdk_events_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpX);
};

typedef MessagePumpX MessagePumpForUI;

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_X_H
