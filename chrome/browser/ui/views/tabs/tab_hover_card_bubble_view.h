// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace gfx {
class ImageSkia;
}

namespace views {
class ImageView;
class Label;
class Widget;
}  // namespace views

class Tab;
struct TabRendererData;

// Dialog that displays an informational hover card containing page information.
class TabHoverCardBubbleView : public views::BubbleDialogDelegateView {
 public:
  explicit TabHoverCardBubbleView(Tab* tab);

  ~TabHoverCardBubbleView() override;

  // Updates card content and anchoring and shows the tab hover card.
  void UpdateAndShow(Tab* tab);

  void UpdateAnchorBounds(gfx::Rect anchor_bounds);

  void FadeOutToHide();

  bool IsFadingOut() const;

  // BubbleDialogDelegateView:
  int GetDialogButtons() const override;

 private:
  friend class TabHoverCardBubbleViewBrowserTest;
  friend class TabHoverCardBubbleViewInteractiveUiTest;
  class WidgetFadeAnimationDelegate;
  class WidgetSlideAnimationDelegate;

  // Get delay in milliseconds based on tab width.
  base::TimeDelta GetDelay(int tab_width) const;

  void FadeInToShow();

  // Updates and formats title and domain with given data.
  void UpdateCardContent(TabRendererData data);

  void UpdatePreviewImage(gfx::ImageSkia preview_image);

  gfx::Size CalculatePreferredSize() const override;

  base::OneShotTimer delayed_show_timer_;

  // Fade animations interfere with browser tests so we disable them in tests.
  static bool disable_animations_for_testing_;
  std::unique_ptr<WidgetFadeAnimationDelegate> fade_animation_delegate_;
  // Used to animate the tab hover card's movement between tabs.
  std::unique_ptr<WidgetSlideAnimationDelegate> slide_animation_delegate_;

  views::Widget* widget_ = nullptr;
  views::Label* title_label_ = nullptr;
  views::Label* domain_label_ = nullptr;
  views::ImageView* preview_image_ = nullptr;

  base::WeakPtrFactory<TabHoverCardBubbleView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TabHoverCardBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
