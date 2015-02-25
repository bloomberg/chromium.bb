// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"

class Browser;
class ElevationIconSetter;
class GlobalErrorWithStandardBubble;

class GlobalErrorBubbleView : public views::ButtonListener,
                              public views::BubbleDelegateView,
                              public GlobalErrorBubbleViewBase {
 public:
  GlobalErrorBubbleView(
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow,
      Browser* browser,
      const base::WeakPtr<GlobalErrorWithStandardBubble>& error);
  ~GlobalErrorBubbleView() override;

  // views::ButtonListener implementation.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::WidgetDelegate implementation.
  base::string16 GetWindowTitle() const override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;
  void WindowClosing() override;

  // views::BubbleDelegateView implementation.
  bool ShouldShowCloseButton() const override;

  // GlobalErrorBubbleViewBase implementation.
  void CloseBubbleView() override;

 private:
  Browser* browser_;
  base::WeakPtr<GlobalErrorWithStandardBubble> error_;

  scoped_ptr<ElevationIconSetter> elevation_icon_setter_;

  DISALLOW_COPY_AND_ASSIGN(GlobalErrorBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
