// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOCK_WINDOW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOCK_WINDOW_H_

#include "base/macros.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class View;
class Widget;
}

namespace chromeos {

// Shows the widget for the WebUI screen locker.
class LockWindow : public views::Widget, public views::WidgetDelegate {
 public:
  explicit LockWindow(views::View* initially_focused_view);
  ~LockWindow() override;

 private:
  // views::WidgetDelegate:
  views::View* GetInitiallyFocusedView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // The view which should be initially focused.
  views::View* initially_focused_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LockWindow);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOCK_WINDOW_H_
