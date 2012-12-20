// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_OVERFLOW_BUBBLE_H_
#define ASH_LAUNCHER_OVERFLOW_BUBBLE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class View;
}

namespace ash {

class LauncherDelegate;
class LauncherModel;

namespace internal {

class LauncherView;

// OverflowBubble displays the overflown launcher items in a bubble.
class OverflowBubble : public views::WidgetObserver {
 public:
  OverflowBubble();
  virtual ~OverflowBubble();

  void Show(LauncherDelegate* delegate,
            LauncherModel* model,
            views::View* anchor,
            int overflow_start_index,
            int overflow_end_index);

  void Hide();

  bool IsShowing() const { return !!bubble_; }

 private:
  // Overridden from views::WidgetObserver:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  views::View* bubble_;  // Owned by views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(OverflowBubble);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_LAUNCHER_OVERFLOW_BUBBLE_H_
