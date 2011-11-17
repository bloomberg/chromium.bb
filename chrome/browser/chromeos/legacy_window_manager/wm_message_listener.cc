// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/legacy_window_manager/wm_message_listener.h"

namespace chromeos {

// static
WmMessageListener* WmMessageListener::GetInstance() {
  static WmMessageListener* instance = NULL;
  if (!instance) {
    instance = Singleton<WmMessageListener>::get();
    MessageLoopForUI::current()->AddObserver(instance);
  }
  return instance;
}

#if defined(TOUCH_UI)
// MessageLoopForUI::Observer overrides.
base::EventStatus WmMessageListener::WillProcessEvent(
    const base::NativeEvent& event) OVERRIDE {
  return base::EVENT_CONTINUE;
}

void WmMessageListener::DidProcessEvent(const base::NativeEvent& event) {
}
#else
void WmMessageListener::WillProcessEvent(GdkEvent* event) {
}

void WmMessageListener::DidProcessEvent(GdkEvent* event) {
  if (event->type == GDK_CLIENT_EVENT) {
    WmIpc::Message message;
    GdkEventClient* client_event = reinterpret_cast<GdkEventClient*>(event);
    WmIpc* wm_ipc = WmIpc::instance();
    if (wm_ipc->DecodeMessage(*client_event, &message))
      ProcessMessage(message, client_event->window);
    else
      wm_ipc->HandleNonChromeClientMessageEvent(*client_event);
  } else if (event->type == GDK_PROPERTY_NOTIFY) {
    GdkEventProperty* property_event =
        reinterpret_cast<GdkEventProperty*>(event);
    if (property_event->window == gdk_get_default_root_window())
      WmIpc::instance()->HandleRootWindowPropertyEvent(*property_event);
  }
}
#endif

WmMessageListener::WmMessageListener() {
}

WmMessageListener::~WmMessageListener() {
}

void WmMessageListener::ProcessMessage(const WmIpc::Message& message,
                                       GdkWindow* window) {
  FOR_EACH_OBSERVER(Observer, observers_, ProcessWmMessage(message, window));
}

}  // namespace chromeos
