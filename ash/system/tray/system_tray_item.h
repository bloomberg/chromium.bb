// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_ITEM_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_ITEM_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/system/user/login_status.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace views {
class View;
}

namespace ash {

class ASH_EXPORT SystemTrayItem {
 public:
  SystemTrayItem();
  virtual ~SystemTrayItem();

  // Create* functions may return NULL if nothing should be displayed for the
  // type of view. The default implementations return NULL.

  // Returns a view to be displayed in the system tray. If this returns NULL,
  // then this item is not displayed in the tray.
  // NOTE: The returned view should almost always be a TrayItemView, which
  // automatically resizes the widget when the size of the view changes, and
  // adds animation when the visibility of the view changes. If a view wants to
  // avoid these behaviour, then it should not be a TrayItemView.
  virtual views::View* CreateTrayView(user::LoginStatus status);

  // Returns a view for the item to be displayed in the list. This view can be
  // displayed with a number of other tray items, so this should not be too
  // big.
  virtual views::View* CreateDefaultView(user::LoginStatus status);

  // Returns a detailed view for the item. This view is displayed standalone.
  virtual views::View* CreateDetailedView(user::LoginStatus status);

  // Returns a notification view for the item. This view is displayed with
  // other notifications and should be the same size as default views.
  virtual views::View* CreateNotificationView(user::LoginStatus status);

  // These functions are called when the corresponding view item is about to be
  // removed. An item should do appropriate cleanup in these functions.
  // The default implementation does nothing.
  virtual void DestroyTrayView();
  virtual void DestroyDefaultView();
  virtual void DestroyDetailedView();
  virtual void DestroyNotificationView();

  // Updates the tray view (if applicable) when the user's login status changes.
  // It is not necessary the update the default or detailed view, since the
  // default/detailed popup is closed when login status changes. The default
  // implementation does nothing.
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status);

  // Shows the detailed view for this item. If the main popup for the tray is
  // currently visible, then making this call would use the existing window to
  // display the detailed item. The detailed item will inherit the bounds of the
  // existing window.
  // If there is no existing view, then this is equivalent to calling
  // PopupDetailedView(0, true).
  void TransitionDetailedView();

  // Pops up the detailed view for this item. An item can request to show its
  // detailed view using this function (e.g. from an observer callback when
  // something, e.g. volume, network availability etc. changes). If
  // |for_seconds| is non-zero, then the popup is closed after the specified
  // time.
  void PopupDetailedView(int for_seconds, bool activate);

  // Continue showing the currently-shown detailed view, if any, for
  // |for_seconds| seconds.  The caller is responsible for checking that the
  // currently-shown view is for this item.
  void SetDetailedViewCloseDelay(int for_seconds);

  // Hides the detailed view for this item.
  void HideDetailedView();

  // Shows a notification for this item.
  void ShowNotificationView();

  // Hides the notification for this item.
  void HideNotificationView();

 private:

  DISALLOW_COPY_AND_ASSIGN(SystemTrayItem);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_ITEM_H_
