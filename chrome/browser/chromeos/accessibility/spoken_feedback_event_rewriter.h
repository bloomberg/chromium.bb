// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SPOKEN_FEEDBACK_EVENT_REWRITER_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SPOKEN_FEEDBACK_EVENT_REWRITER_H_

#include <vector>

#include "base/macros.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/events/event_rewriter.h"

namespace ui {
class KeyEvent;
}

// Receives requests for spoken feedback enabled state and command dispatch.
class SpokenFeedbackEventRewriterDelegate
    : public content::WebContentsDelegate {
 public:
  SpokenFeedbackEventRewriterDelegate();
  ~SpokenFeedbackEventRewriterDelegate() override {}

  // Returns true when ChromeVox is enabled.
  virtual bool IsSpokenFeedbackEnabled() const;

  // Returns true when |key_event| is dispatched to ChromeVox.
  virtual bool DispatchKeyEventToChromeVox(const ui::KeyEvent& key_event);

  // WebContentsDelegate:
  void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SpokenFeedbackEventRewriterDelegate);
};

// SpokenFeedbackEventRewriter discards all keyboard events mapped by the spoken
// feedback manifest commands block. It dispatches the associated command name
// directly to spoken feedback. This only occurs whenever spoken feedback is
// enabled.
class SpokenFeedbackEventRewriter : public ui::EventRewriter {
 public:
  SpokenFeedbackEventRewriter();
  ~SpokenFeedbackEventRewriter() override;

  void SetDelegateForTest(
      scoped_ptr<SpokenFeedbackEventRewriterDelegate> delegate);

 private:
  // EventRewriter:
  ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      scoped_ptr<ui::Event>* new_event) override;
  ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event,
      scoped_ptr<ui::Event>* new_event) override;

  // Active delegate (used for testing).
  scoped_ptr<SpokenFeedbackEventRewriterDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(SpokenFeedbackEventRewriter);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SPOKEN_FEEDBACK_EVENT_REWRITER_H_
