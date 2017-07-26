// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_PROMO_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_PROMO_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"

namespace gfx {
class Rect;
}

namespace ui {
class MouseEvent;
}

// The NewTabPromo is a bubble anchored to the right of the new tab button. It
// draws users attention to the newtab button. It is called by the NewTabButton
// when prompted by the NewTabFeatureEngagementTracker.
class NewTabPromo : public views::BubbleDialogDelegateView {
 public:
  // Returns a self owned raw pointer to the NewTabButton.
  static NewTabPromo* CreateSelfOwned(const gfx::Rect& anchor_rect);

 private:
  // Constructs NewTabPromo. Anchors the bubble to the NewTabButton.
  // The bubble widget and promo are owned by their native widget.
  explicit NewTabPromo(const gfx::Rect& anchor_rect);
  ~NewTabPromo() override;

  // BubbleDialogDelegateView:
  int GetDialogButtons() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // Closes the New Tab Promo.
  void CloseBubble();

  // Starts a timer which will close the bubble.
  void StartAutoCloseTimer(base::TimeDelta auto_close_duration);

  // Timer used to auto close the bubble.
  base::OneShotTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(NewTabPromo);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_PROMO_H_
