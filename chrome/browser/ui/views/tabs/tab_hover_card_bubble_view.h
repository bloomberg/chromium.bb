// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class Label;
class Widget;
}  // namespace views
struct TabRendererData;

// Dialog that displays an informational hover card containing page information.
class TabHoverCardBubbleView : public views::BubbleDialogDelegateView {
 public:
  TabHoverCardBubbleView(views::View* anchor_view, TabRendererData data);

  ~TabHoverCardBubbleView() override;

  // Creates (if not already created) and shows the widget.
  void Show();

  void Hide();

  // Updates and formats title and domain with given data.
  void UpdateCardContent(TabRendererData data);

  // Updates where the card should be anchored.
  void UpdateCardAnchor(View* tab);

  // BubbleDialogDelegateView:
  int GetDialogButtons() const override;

 private:
  friend class TabHoverCardBubbleViewBrowserTest;

  views::Widget* widget_ = nullptr;
  views::Label* title_label_;
  views::Label* domain_label_;

  gfx::Size CalculatePreferredSize() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
