// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EVENTS_KEYBOARD_DRIVEN_EVENT_REWRITER_H_
#define CHROME_BROWSER_CHROMEOS_EVENTS_KEYBOARD_DRIVEN_EVENT_REWRITER_H_

#include "ui/events/event_rewriter.h"

namespace chromeos {

// KeyboardDrivenEventRewriter removes the modifier flags from
// Shift+<Arrow keys|Enter|F6> key events. This mapping only happens
// on login screen and only when the keyboard driven oobe is enabled.
class KeyboardDrivenEventRewriter : public ui::EventRewriter {
 public:
  KeyboardDrivenEventRewriter();
  virtual ~KeyboardDrivenEventRewriter();

  // Calls Rewrite for testing.
  ui::EventRewriteStatus RewriteForTesting(const ui::Event& event,
                                           scoped_ptr<ui::Event>* new_event);

  // EventRewriter overrides:
  virtual ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      scoped_ptr<ui::Event>* new_event) OVERRIDE;
  virtual ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event,
      scoped_ptr<ui::Event>* new_event) OVERRIDE;

 private:
  ui::EventRewriteStatus Rewrite(const ui::Event& event,
                                 scoped_ptr<ui::Event>* new_event);

  DISALLOW_COPY_AND_ASSIGN(KeyboardDrivenEventRewriter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EVENTS_KEYBOARD_DRIVEN_EVENT_REWRITER_H_
