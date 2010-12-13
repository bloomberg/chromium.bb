// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wm_message_listener.h"

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
  }
}

WmMessageListener::WmMessageListener() {
}

WmMessageListener::~WmMessageListener() {
}

void WmMessageListener::ProcessMessage(const WmIpc::Message& message,
                                       GdkWindow* window) {
  FOR_EACH_OBSERVER(Observer, observers_, ProcessWmMessage(message, window));
}

}  // namespace chromeos
