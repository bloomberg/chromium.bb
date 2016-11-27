// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NEW_BACK_SHORTCUT_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_NEW_BACK_SHORTCUT_BUBBLE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/gfx/animation/animation_delegate.h"

class ExclusiveAccessBubbleViewsContext;
class SubtleNotificationView;

namespace gfx {
class Animation;
class Rect;
class SlideAnimation;
}

namespace views {
class Widget;
}

// NewBackShortcutBubble shows a short-lived notification along the top of the
// screen when the user presses the old Back/Forward shortcut, telling them how
// to use the new shortcut. This will only be available for a few milestones to
// let users adapt.
// TODO(mgiuca): Remove this in M54 (https://crbug.com/610039).
class NewBackShortcutBubble : public gfx::AnimationDelegate {
 public:
  explicit NewBackShortcutBubble(ExclusiveAccessBubbleViewsContext* context);
  ~NewBackShortcutBubble() override;

  // Returns whether the UI is currently visible.
  bool IsVisible() const;

  // Ensures the UI is displaying the correct shortcut for forward/back based on
  // |forward|, and resets the hide timer.
  void UpdateContent(bool forward);

  // Fades out the UI immediately.
  void Hide();

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  gfx::Rect GetPopupRect(bool ignore_animation_state) const;
  void OnTimerElapsed();

  ExclusiveAccessBubbleViewsContext* const bubble_view_context_;

  // Animation controlling showing/hiding of the bubble.
  std::unique_ptr<gfx::SlideAnimation> animation_;

  // Timer before the bubble disappears.
  base::OneShotTimer hide_timeout_;

  // The contents of the popup.
  SubtleNotificationView* const view_;

  views::Widget* const popup_;

  DISALLOW_COPY_AND_ASSIGN(NewBackShortcutBubble);
};

#endif  // CHROME_BROWSER_UI_VIEWS_NEW_BACK_SHORTCUT_BUBBLE_H_
