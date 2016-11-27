// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_VIEWS_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

class ToolbarActionsBarBubbleDelegate;
class ToolbarActionsBarBubbleViewsTest;

namespace views {
class Label;
class Link;
}

class ToolbarActionsBarBubbleViews : public views::BubbleDialogDelegateView,
                                     public views::LinkListener {
 public:
  ToolbarActionsBarBubbleViews(
      views::View* anchor_view,
      std::unique_ptr<ToolbarActionsBarBubbleDelegate> delegate);
  ~ToolbarActionsBarBubbleViews() override;

  void Show();

  const views::Label* item_list() const { return item_list_; }
  const views::Link* learn_more_button() const { return link_; }

 private:
  friend class ToolbarActionsBarBubbleViewsTest;

  // views::BubbleDialogDelegateView:
  base::string16 GetWindowTitle() const override;
  views::View* CreateExtraView() override;
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;
  int GetDialogButtons() const override;
  int GetDefaultDialogButton() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  void Init() override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  std::unique_ptr<ToolbarActionsBarBubbleDelegate> delegate_;
  views::Label* item_list_;
  views::Link* link_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsBarBubbleViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_VIEWS_H_
