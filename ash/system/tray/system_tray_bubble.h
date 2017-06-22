// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_BUBBLE_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_BUBBLE_H_

#include <map>
#include <memory>
#include <vector>

#include "ash/login_status.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/views/bubble/tray_bubble_view.h"

namespace ash {
class SystemTray;
class SystemTrayItem;

class SystemTrayBubble {
 public:
  enum BubbleType { BUBBLE_TYPE_DEFAULT, BUBBLE_TYPE_DETAILED };

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
                LoginStatus login_status,
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

  // Records metrics for visible system menu rows. Only implemented for the
  // BUBBLE_TYPE_DEFAULT BubbleType.
  void RecordVisibleRowMetrics();

 private:
  // Updates the bottom padding of the |bubble_view_| based on the
  // |bubble_type_|.
  void UpdateBottomPadding();
  void CreateItemViews(LoginStatus login_status);

  ash::SystemTray* tray_;
  views::TrayBubbleView* bubble_view_;
  std::vector<ash::SystemTrayItem*> items_;
  BubbleType bubble_type_;

  // Tracks the views created in the last call to CreateItemViews().
  std::map<SystemTrayItem::UmaType, views::View*> tray_item_view_map_;

  int autoclose_delay_;
  base::OneShotTimer autoclose_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubble);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_BUBBLE_H_
