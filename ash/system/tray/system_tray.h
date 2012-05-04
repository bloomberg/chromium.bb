// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
#pragma once

#include "ash/launcher/background_animator.h"
#include "ash/ash_export.h"
#include "ash/system/tray/tray_views.h"
#include "ash/system/user/login_status.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "ui/views/view.h"

#include <vector>

namespace ash {

class AccessibilityObserver;
class AudioObserver;
class BluetoothObserver;
class BrightnessObserver;
class CapsLockObserver;
class ClockObserver;
class DriveObserver;
class IMEObserver;
class NetworkObserver;
class PowerStatusObserver;
class UpdateObserver;
class UserObserver;

class SystemTrayItem;

namespace internal {
class SystemTrayBackground;
class SystemTrayBubble;
}

class ASH_EXPORT SystemTray : NON_EXPORTED_BASE(
                                  public internal::ActionableView),
                              public internal::BackgroundAnimatorDelegate {
 public:
  SystemTray();
  virtual ~SystemTray();

  // Creates the default set of items for the sytem tray.
  void CreateItems();

  // Creates the widget for the tray.
  void CreateWidget();

  // Adds a new item in the tray.
  void AddTrayItem(SystemTrayItem* item);

  // Removes an existing tray item.
  void RemoveTrayItem(SystemTrayItem* item);

  // Shows the default view of all items.
  void ShowDefaultView();

  // Shows details of a particular item. If |close_delay_in_seconds| is
  // non-zero, then the view is automatically closed after the specified time.
  void ShowDetailedView(SystemTrayItem* item,
                        int close_delay_in_seconds,
                        bool activate);

  // Continue showing the existing detailed view, if any, for |close_delay|
  // seconds.
  void SetDetailedViewCloseDelay(int close_delay);

  // Updates the items when the login status of the system changes.
  void UpdateAfterLoginStatusChange(user::LoginStatus login_status);

  // Sets whether the tray paints a background. Default is true, but is set to
  // false if a window overlaps the shelf.
  void SetPaintsBackground(
      bool value,
      internal::BackgroundAnimator::ChangeType change_type);

  // Returns true if the launcher should show.
  bool should_show_launcher() const {
    return bubble_.get() && should_show_launcher_;
  }

  views::Widget* widget() const { return widget_; }

  AccessibilityObserver* accessibility_observer() const {
    return accessibility_observer_;
  }
  AudioObserver* audio_observer() const {
    return audio_observer_;
  }
  BluetoothObserver* bluetooth_observer() const {
    return bluetooth_observer_;
  }
  BrightnessObserver* brightness_observer() const {
    return brightness_observer_;
  }
  CapsLockObserver* caps_lock_observer() const {
    return caps_lock_observer_;
  }
  ClockObserver* clock_observer() const {
    return clock_observer_;
  }
  DriveObserver* drive_observer() const {
    return drive_observer_;
  }
  IMEObserver* ime_observer() const {
    return ime_observer_;
  }
  NetworkObserver* network_observer() const {
    return network_observer_;
  }
  PowerStatusObserver* power_status_observer() const {
    return power_status_observer_;
  }
  UpdateObserver* update_observer() const {
    return update_observer_;
  }
  UserObserver* user_observer() const {
    return user_observer_;
  }

  // Accessors for testing.

  // Returns true if the bubble exists.
  bool CloseBubbleForTest() const;

 private:
  friend class internal::SystemTrayBubble;

  // Called when the widget associated with |bubble| closes. |bubble| should
  // always == |bubble_|. This triggers destroying |bubble_| and hiding the
  // launcher if necessary.
  void RemoveBubble(internal::SystemTrayBubble* bubble);

  const ScopedVector<SystemTrayItem>& items() const { return items_; }

  void ShowItems(const std::vector<SystemTrayItem*>& items,
                 bool details,
                 bool activate);

  // Overridden from internal::ActionableView.
  virtual bool PerformAction(const views::Event& event) OVERRIDE;

  // Overridden from views::View.
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;
  virtual void AboutToRequestFocusFromTabTraversal(bool reverse) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from internal::BackgroundAnimatorDelegate.
  virtual void UpdateBackground(int alpha) OVERRIDE;

  ScopedVector<SystemTrayItem> items_;

  // The container for all the tray views of the items.
  views::View* container_;

  // These observers are not owned by the tray.
  AccessibilityObserver* accessibility_observer_;
  AudioObserver* audio_observer_;
  BluetoothObserver* bluetooth_observer_;
  BrightnessObserver* brightness_observer_;
  CapsLockObserver* caps_lock_observer_;
  ClockObserver* clock_observer_;
  DriveObserver* drive_observer_;
  IMEObserver* ime_observer_;
  NetworkObserver* network_observer_;
  PowerStatusObserver* power_status_observer_;
  UpdateObserver* update_observer_;
  UserObserver* user_observer_;

  // The widget hosting the tray.
  views::Widget* widget_;

  // The popup widget and the delegate.
  scoped_ptr<internal::SystemTrayBubble> bubble_;

  // Owned by the view it's installed on.
  internal::SystemTrayBackground* background_;

  // See description agove getter.
  bool should_show_launcher_;

  internal::BackgroundAnimator hide_background_animator_;
  internal::BackgroundAnimator hover_background_animator_;

  DISALLOW_COPY_AND_ASSIGN(SystemTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
