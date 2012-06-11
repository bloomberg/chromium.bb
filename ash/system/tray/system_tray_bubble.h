// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_BUBBLE_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_BUBBLE_H_
#pragma once

#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/user/login_status.h"
#include "ash/wm/shelf_auto_hide_behavior.h"
#include "base/base_export.h"
#include "base/timer.h"
#include "ui/aura/event_filter.h"
#include "ui/views/widget/widget.h"

#include <vector>

namespace aura {
class LocatedEvent;
}

namespace ash {

class SystemTray;
class SystemTrayItem;

namespace internal {

class SystemTrayBubble : public aura::EventFilter,
                         public views::Widget::Observer,
                         public TrayBubbleView::Host {
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

  struct InitParams {
    InitParams(AnchorType anchor_type, ShelfAlignment shelf_alignmen);

    views::View* anchor;
    AnchorType anchor_type;
    bool can_activate;
    ash::user::LoginStatus login_status;
    int arrow_offset;
    int max_height;
  };

  SystemTrayBubble(ash::SystemTray* tray,
                   const std::vector<ash::SystemTrayItem*>& items,
                   BubbleType bubble_type);
  virtual ~SystemTrayBubble();

  // Change the items displayed in the bubble.
  void UpdateView(const std::vector<ash::SystemTrayItem*>& items,
                  BubbleType bubble_type);

  // Creates |bubble_view_| and a child views for each member of |items_|.
  // Also creates |bubble_widget_| and sets up animations.
  void InitView(const InitParams& init_params);

  // Overridden from TrayBubbleView::Host.
  virtual void BubbleViewDestroyed() OVERRIDE;
  virtual gfx::Rect GetAnchorRect() const OVERRIDE;
  virtual void OnMouseEnteredView() OVERRIDE;
  virtual void OnMouseExitedView() OVERRIDE;

  BubbleType bubble_type() const { return bubble_type_; }
  TrayBubbleView* bubble_view() const { return bubble_view_; }

  void DestroyItemViews();
  void StartAutoCloseTimer(int seconds);
  void StopAutoCloseTimer();
  void RestartAutoCloseTimer();
  void Close();

 private:
  void CreateItemViews(user::LoginStatus login_status);

  // Closes the bubble if the event happened outside the bounds.
  // Returns true if the event should be stopped from being propagated farther.
  bool ProcessLocatedEvent(const aura::LocatedEvent& event);

  // Overridden from aura::EventFilter.
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

  // Overridden from views::Widget::Observer.
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  ash::SystemTray* tray_;
  TrayBubbleView* bubble_view_;
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
