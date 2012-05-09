// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_BUBBLE_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_BUBBLE_H_
#pragma once

#include "ash/system/user/login_status.h"
#include "base/base_export.h"
#include "base/message_pump_observer.h"
#include "base/timer.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/widget/widget.h"

#include <vector>

namespace ash {

class SystemTray;
class SystemTrayItem;

namespace internal {

class SystemTrayBubble;

class SystemTrayBubbleView : public views::BubbleDelegateView {
 public:
  SystemTrayBubbleView(views::View* anchor,
                       SystemTrayBubble* host,
                       bool can_activate);
  virtual ~SystemTrayBubbleView();

  void SetBubbleBorder(views::BubbleBorder* border);

  void UpdateAnchor();

  // Called when the host is destroyed.
  void reset_host() { host_ = NULL; }

 private:
  // Overridden from views::BubbleDelegateView.
  virtual void Init() OVERRIDE;
  virtual gfx::Rect GetAnchorRect() OVERRIDE;
  // Overridden from views::View.
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual bool CanActivate() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;

  SystemTrayBubble* host_;
  bool can_activate_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubbleView);
};

class SystemTrayBubble : public base::MessagePumpObserver,
                         public views::Widget::Observer {
 public:
  enum BubbleType {
    BUBBLE_TYPE_DEFAULT,
    BUBBLE_TYPE_DETAILED,
    BUBBLE_TYPE_NOTIFICATION
  };

  enum AnchorType {
    ANCHOR_TYPE_TRAY,
    ANCHOR_TYPE_BUBBLE
  };

  SystemTrayBubble(ash::SystemTray* tray,
                   const std::vector<ash::SystemTrayItem*>& items,
                   BubbleType bubble_type);
  virtual ~SystemTrayBubble();

  // Creates |bubble_view_| and a child views for each member of |items_|.
  // Also creates |bubble_widget_| and sets up animations.
  void InitView(views::View* anchor,
                AnchorType anchor_type,
                bool can_activate,
                ash::user::LoginStatus login_status);

  gfx::Rect GetAnchorRect() const;

  BubbleType bubble_type() const { return bubble_type_; }
  SystemTrayBubbleView* bubble_view() const { return bubble_view_; }

  void DestroyItemViews();
  void StartAutoCloseTimer(int seconds);
  void StopAutoCloseTimer();
  void RestartAutoCloseTimer();
  void Close();

 private:
  // Overridden from base::MessagePumpObserver.
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE;
  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE;
  // Overridden from views::Widget::Observer.
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;
  virtual void OnWidgetVisibilityChanged(views::Widget* widget,
                                         bool visible) OVERRIDE;

  ash::SystemTray* tray_;
  SystemTrayBubbleView* bubble_view_;
  views::Widget* bubble_widget_;
  std::vector<ash::SystemTrayItem*> items_;
  BubbleType bubble_type_;
  AnchorType anchor_type_;

  int autoclose_delay_;
  base::OneShotTimer<SystemTrayBubble> autoclose_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubble);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_BUBBLE_H_
