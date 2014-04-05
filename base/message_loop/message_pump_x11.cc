// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_x11.h"

#include <glib.h>
#include <X11/X.h>
#include <X11/extensions/XInput2.h>
#include <X11/XKBlib.h>

#include "base/basictypes.h"
#include "base/message_loop/message_loop.h"

namespace base {

namespace {

// The connection is essentially a global that's accessed through a static
// method and destroyed whenever ~MessagePumpX11() is called. We do this
// for historical reasons so user code can call
// MessagePumpForUI::GetDefaultXDisplay() where MessagePumpForUI is a typedef
// to whatever type in the current build.
//
// TODO(erg): This can be changed to something more sane like
// MessagePumpX11::Current()->display() once MessagePumpGtk goes away.
Display* g_xdisplay = NULL;

}  // namespace

MessagePumpX11::MessagePumpX11() : MessagePumpGlib() {}

MessagePumpX11::~MessagePumpX11() {
  if (g_xdisplay) {
    XCloseDisplay(g_xdisplay);
    g_xdisplay = NULL;
  }
}

// static
Display* MessagePumpX11::GetDefaultXDisplay() {
  if (!g_xdisplay)
    g_xdisplay = XOpenDisplay(NULL);
  return g_xdisplay;
}

#if defined(TOOLKIT_GTK)
// static
MessagePumpX11* MessagePumpX11::Current() {
  MessageLoop* loop = MessageLoop::current();
  return static_cast<MessagePumpX11*>(loop->pump_gpu());
}
#else
// static
MessagePumpX11* MessagePumpX11::Current() {
  MessageLoopForUI* loop = MessageLoopForUI::current();
  return static_cast<MessagePumpX11*>(loop->pump_ui());
}
#endif

void MessagePumpX11::AddObserver(MessagePumpObserver* observer) {
  observers_.AddObserver(observer);
}

void MessagePumpX11::RemoveObserver(MessagePumpObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool MessagePumpX11::WillProcessXEvent(XEvent* xevent) {
  if (!observers_.might_have_observers())
    return false;
  ObserverListBase<MessagePumpObserver>::Iterator it(observers_);
  MessagePumpObserver* obs;
  while ((obs = it.GetNext()) != NULL) {
    if (obs->WillProcessEvent(xevent))
      return true;
  }
  return false;
}

void MessagePumpX11::DidProcessXEvent(XEvent* xevent) {
  FOR_EACH_OBSERVER(MessagePumpObserver, observers_, DidProcessEvent(xevent));
}

}  // namespace base
