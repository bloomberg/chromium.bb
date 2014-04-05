// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_X11_H
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_X11_H

#include <bitset>
#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_dispatcher.h"
#include "base/message_loop/message_pump_glib.h"
#include "base/message_loop/message_pump_observer.h"
#include "base/observer_list.h"

// It would be nice to include the X11 headers here so that we use Window
// instead of its typedef of unsigned long, but we can't because everything in
// chrome includes us through base/message_loop/message_loop.h, and X11's crappy
// #define heavy headers muck up half of chrome.

typedef struct _GPollFD GPollFD;
typedef struct _GSource GSource;
typedef struct _XDisplay Display;

namespace base {

// This class implements a message-pump for dispatching X events.
//
// If there's a current dispatcher given through RunWithDispatcher(), that
// dispatcher receives events. Otherwise, we route to messages to dispatchers
// who have subscribed to messages from a specific X11 window.
class BASE_EXPORT MessagePumpX11 : public MessagePumpGlib {
 public:
  MessagePumpX11();
  virtual ~MessagePumpX11();

  // Returns default X Display.
  static Display* GetDefaultXDisplay();

  // Returns the UI or GPU message pump.
  static MessagePumpX11* Current();

  // Adds an Observer, which will start receiving notifications immediately.
  void AddObserver(MessagePumpObserver* observer);

  // Removes an Observer.  It is safe to call this method while an Observer is
  // receiving a notification callback.
  void RemoveObserver(MessagePumpObserver* observer);

  // Sends the event to the observers.
  void WillProcessXEvent(XEvent* xevent);
  void DidProcessXEvent(XEvent* xevent);

 private:
  // List of observers.
  ObserverList<MessagePumpObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpX11);
};

#if !defined(TOOLKIT_GTK)
typedef MessagePumpX11 MessagePumpForUI;
#endif

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_X11_H
