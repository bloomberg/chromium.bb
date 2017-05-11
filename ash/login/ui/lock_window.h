// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_WINDOW_H_
#define ASH_LOGIN_UI_LOCK_WINDOW_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class View;
class Widget;
}

namespace ash {

// Shows the widget for the lock screen.
class ASH_EXPORT LockWindow : public views::Widget,
                              public views::WidgetDelegate {
 public:
  LockWindow();
  ~LockWindow() override;

 private:
  // views::WidgetDelegate:
  views::View* GetInitiallyFocusedView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  DISALLOW_COPY_AND_ASSIGN(LockWindow);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_WINDOW_H_
