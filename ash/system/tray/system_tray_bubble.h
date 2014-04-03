// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_BUBBLE_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_BUBBLE_H_

#include "ash/system/user/login_status.h"
#include "base/base_export.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/bubble/tray_bubble_view.h"

#include <vector>

namespace ash {
class SystemTray;
class SystemTrayItem;

class SystemTrayBubble {
 public:
  enum BubbleType {
    BUBBLE_TYPE_DEFAULT,
    BUBBLE_TYPE_DETAILED,
    BUBBLE_TYPE_NOTIFICATION
  };

  SystemTrayBubble(ash::SystemTray* tray,
                   const std::vector<ash::SystemTrayItem*>& items,
                   BubbleType bubble_type);
  virtual ~SystemTrayBubble();

  // Change the items displayed in the bubble.
  void UpdateView(const std::vector<ash::SystemTrayItem*>& items,
                  BubbleType bubble_type);

  // Creates |bubble_view_| and a child views for each member of |items_|.
  // Also creates |bubble_wrapper_|. |init_params| may be modified.
  void InitView(views::View* anchor,
                user::LoginStatus login_status,
                views::TrayBubbleView::InitParams* init_params);

  // Focus the default item if no item is focused. Othewise, do nothing.
  void FocusDefaultIfNeeded();

  BubbleType bubble_type() const { return bubble_type_; }
  views::TrayBubbleView* bubble_view() const { return bubble_view_; }

  void DestroyItemViews();
  void BubbleViewDestroyed();
  void StartAutoCloseTimer(int seconds);
  void StopAutoCloseTimer();
  void RestartAutoCloseTimer();
  void Close();
  void SetVisible(bool is_visible);
  bool IsVisible();

  // Returns true if any of the SystemTrayItems return true from
  // ShouldShowShelf().
  bool ShouldShowShelf() const;

 private:
  void CreateItemViews(user::LoginStatus login_status);

  ash::SystemTray* tray_;
  views::TrayBubbleView* bubble_view_;
  std::vector<ash::SystemTrayItem*> items_;
  BubbleType bubble_type_;

  int autoclose_delay_;
  base::OneShotTimer<SystemTrayBubble> autoclose_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubble);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_BUBBLE_H_
