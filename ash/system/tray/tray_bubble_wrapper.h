// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_
#define ASH_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class TrayBubbleView;
}

namespace ash {
namespace internal {

class TrayBackgroundView;
class TrayEventFilter;

// Creates and manages the Widget and EventFilter components of a bubble.

class TrayBubbleWrapper : public views::WidgetObserver {
 public:
  TrayBubbleWrapper(TrayBackgroundView* tray,
                    views::TrayBubbleView* bubble_view);
  virtual ~TrayBubbleWrapper();

  // views::WidgetObserver overrides:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  TrayBackgroundView* tray() { return tray_; }
  views::TrayBubbleView* bubble_view() { return bubble_view_; }
  views::Widget* bubble_widget() { return bubble_widget_; }

 private:
  TrayBackgroundView* tray_;
  views::TrayBubbleView* bubble_view_;  // unowned
  views::Widget* bubble_widget_;
  scoped_ptr<TrayEventFilter> tray_event_filter_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleWrapper);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_
