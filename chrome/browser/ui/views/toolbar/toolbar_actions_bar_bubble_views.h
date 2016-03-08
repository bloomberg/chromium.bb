// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_VIEWS_H_

#include "base/macros.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"

class Profile;
class ToolbarActionsBarBubbleDelegate;
class ToolbarActionsBarBubbleViewsUnitTest;

namespace views {
class Label;
class LabelButton;
}

// TODO(devlin): It might be best for this to combine with
// ExtensionMessageBubbleView, similar to what we do on Mac.
class ToolbarActionsBarBubbleViews : public views::BubbleDelegateView,
                                     public views::ButtonListener {
 public:
  ToolbarActionsBarBubbleViews(
      views::View* anchor_view,
      scoped_ptr<ToolbarActionsBarBubbleDelegate> delegate);
  ~ToolbarActionsBarBubbleViews() override;

  void Show();

  const views::Label* heading_label() const { return heading_label_; }
  const views::Label* content_label() const { return content_label_; }
  const views::LabelButton* action_button() const { return action_button_; }
  const views::LabelButton* dismiss_button() const { return dismiss_button_; }

 private:
  // views::BubbleDelegateView:
  void Init() override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  scoped_ptr<ToolbarActionsBarBubbleDelegate> delegate_;

  views::Label* heading_label_;
  views::Label* content_label_;

  views::LabelButton* dismiss_button_;
  views::LabelButton* action_button_;

  // Whether or not the user acknowledged the bubble by clicking the "ok"
  // button.
  bool acknowledged_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsBarBubbleViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_VIEWS_H_
