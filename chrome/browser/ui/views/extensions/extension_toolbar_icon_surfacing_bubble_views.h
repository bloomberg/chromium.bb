// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_TOOLBAR_ICON_SURFACING_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_TOOLBAR_ICON_SURFACING_BUBBLE_VIEWS_H_

#include "base/macros.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"

class Profile;
class ToolbarActionsBarBubbleDelegate;

// TODO(devlin): It might be best for this to combine with
// ExtensionMessageBubbleView, similar to what we do on Mac.
class ExtensionToolbarIconSurfacingBubble : public views::BubbleDelegateView,
                                            public views::ButtonListener {
 public:
  ExtensionToolbarIconSurfacingBubble(
      views::View* anchor_view,
      scoped_ptr<ToolbarActionsBarBubbleDelegate> delegate);
  ~ExtensionToolbarIconSurfacingBubble() override;

  void Show();

 private:
  // views::BubbleDelegateView:
  void Init() override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  scoped_ptr<ToolbarActionsBarBubbleDelegate> delegate_;

  // Whether or not the user acknowledged the bubble by clicking the "ok"
  // button.
  bool acknowledged_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionToolbarIconSurfacingBubble);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_TOOLBAR_ICON_SURFACING_BUBBLE_VIEWS_H_
