// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SELECT_TO_SPEAK_EVENT_REWRITER_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SELECT_TO_SPEAK_EVENT_REWRITER_H_

#include <memory>

#include "base/macros.h"
#include "ui/aura/window.h"
#include "ui/events/event_rewriter.h"

namespace ui {
class KeyEvent;
class MouseEvent;
}  // namespace ui

class SelectToSpeakEventDelegateForTesting {
 public:
  virtual ~SelectToSpeakEventDelegateForTesting() = default;

  virtual void OnForwardEventToSelectToSpeakExtension(
      const ui::MouseEvent& event) = 0;
};

class SelectToSpeakEventRewriter : public ui::EventRewriter {
 public:
  explicit SelectToSpeakEventRewriter(aura::Window* root_window);
  ~SelectToSpeakEventRewriter() override;

  void CaptureForwardedEventsForTesting(
      SelectToSpeakEventDelegateForTesting* delegate);

 private:
  // Returns true if Select to Speak is enabled.
  bool IsSelectToSpeakEnabled();

  // Returns true if the event was consumed and should be canceled.
  bool OnKeyEvent(const ui::KeyEvent* event);

  // Returns true if the event was consumed and should be canceled.
  bool OnMouseEvent(const ui::MouseEvent* event);

  // Converts an event in pixels to the same event in DIPs.
  void ConvertMouseEventToDIPs(ui::MouseEvent* mouse_event);

  // EventRewriter:
  ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      std::unique_ptr<ui::Event>* new_event) override;
  ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event,
      std::unique_ptr<ui::Event>* new_event) override;

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
  aura::Window* root_window_;

  SelectToSpeakEventDelegateForTesting* event_delegate_for_testing_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SelectToSpeakEventRewriter);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SELECT_TO_SPEAK_EVENT_REWRITER_H_
