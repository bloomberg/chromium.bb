// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SELECT_TO_SPEAK_EVENT_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SELECT_TO_SPEAK_EVENT_HANDLER_H_

#include "base/macros.h"
#include "ui/events/event_handler.h"

namespace chromeos {

// Intercepts mouse events while the Search key is held down, and sends
// accessibility events to the Select-to-speak extension instead.
class SelectToSpeakEventHandler : public ui::EventHandler {
 public:
  SelectToSpeakEventHandler();
  ~SelectToSpeakEventHandler() override;

 private:
  // EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  void CancelEvent(ui::Event* event);
  void SendCancelAXEvent();

  enum State {
    // Neither the Search key nor the mouse button are down.
    INACTIVE,

    // The Search key is down but the mouse button is not.
    SEARCH_DOWN,

    // The user held down Search and clicked the mouse button. We're capturing
    // all events from now on until either Search or the mouse button is
    // released.
    CAPTURING,

    // The mouse was released, but Search is still held down. If the user
    // clicks again, we'll go back to the state CAPTURING. This is different
    // than the state SEARCH_DOWN because we know the user clicked at least
    // once, so when Search is released we'll handle that event too, so as
    // to not trigger opening the Search UI.
    MOUSE_RELEASED,

    // The Search key was released while the mouse was still down, cancelling
    // the Select-to-Speak event. Stay in this mode until the mouse button
    // is released, too.
    WAIT_FOR_MOUSE_RELEASE
  };

  State state_ = INACTIVE;

  int last_view_storage_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SelectToSpeakEventHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SELECT_TO_SPEAK_EVENT_HANDLER_H_
