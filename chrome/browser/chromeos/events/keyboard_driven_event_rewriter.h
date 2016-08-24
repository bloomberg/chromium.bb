// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EVENTS_KEYBOARD_DRIVEN_EVENT_REWRITER_H_
#define CHROME_BROWSER_CHROMEOS_EVENTS_KEYBOARD_DRIVEN_EVENT_REWRITER_H_

#include "base/macros.h"
#include "ui/events/event_rewriter.h"

namespace chromeos {

// KeyboardDrivenEventRewriter removes the modifier flags from
// Shift+<Arrow keys|Enter|F6> key events. This mapping only happens
// on login screen and only when the keyboard driven oobe is enabled.
class KeyboardDrivenEventRewriter : public ui::EventRewriter {
 public:
  KeyboardDrivenEventRewriter();
  ~KeyboardDrivenEventRewriter() override;

  static KeyboardDrivenEventRewriter* GetInstance();

  // Calls Rewrite for testing.
  ui::EventRewriteStatus RewriteForTesting(
      const ui::Event& event,
      std::unique_ptr<ui::Event>* new_event);

  // EventRewriter overrides:
  ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      std::unique_ptr<ui::Event>* new_event) override;
  ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event,
      std::unique_ptr<ui::Event>* new_event) override;

  // Allow setting Shift + Arrow keys rewritten to Tab/Shift-Tab keys |enabled|.
  void SetArrowToTabRewritingEnabled(bool rewritten_to_tab) {
    rewritten_to_tab_ = rewritten_to_tab;
  }

 private:
  ui::EventRewriteStatus Rewrite(const ui::Event& event,
                                 std::unique_ptr<ui::Event>* new_event);

  // If true, Shift + Arrow keys are rewritten to Tab/Shift-Tab keys.
  bool rewritten_to_tab_ = false;

  DISALLOW_COPY_AND_ASSIGN(KeyboardDrivenEventRewriter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EVENTS_KEYBOARD_DRIVEN_EVENT_REWRITER_H_
