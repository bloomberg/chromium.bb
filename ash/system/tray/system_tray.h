// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_

#include "ash/ash_export.h"
#include "ash/system/power/power_supply_status.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_views.h"
#include "ash/system/user/login_status.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "ui/views/view.h"

#include <map>
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
class LocaleObserver;
class NetworkObserver;
class SmsObserver;
class PowerStatusObserver;
class UpdateObserver;
class UserObserver;

class SystemTrayItem;

namespace internal {
class SystemTrayBubble;
class SystemTrayContainer;
class TrayGestureHandler;
}

// There are different methods for creating bubble views.
enum BubbleCreationType {
  BUBBLE_CREATE_NEW,    // Closes any existing bubble and creates a new one.
  BUBBLE_USE_EXISTING,  // Uses any existing bubble, or creates a new one.
};

class ASH_EXPORT SystemTray : public internal::TrayBackgroundView {
 public:
  explicit SystemTray(internal::StatusAreaWidget* status_area_widget);
  virtual ~SystemTray();

  // Creates the default set of items for the sytem tray.
  void CreateItems();

  // Adds a new item in the tray.
  void AddTrayItem(SystemTrayItem* item);

  // Removes an existing tray item.
  void RemoveTrayItem(SystemTrayItem* item);

  // Shows the default view of all items.
  void ShowDefaultView(BubbleCreationType creation_type);

  // Shows details of a particular item. If |close_delay_in_seconds| is
  // non-zero, then the view is automatically closed after the specified time.
  void ShowDetailedView(SystemTrayItem* item,
                        int close_delay_in_seconds,
                        bool activate,
                        BubbleCreationType creation_type);

  // Continue showing the existing detailed view, if any, for |close_delay|
  // seconds.
  void SetDetailedViewCloseDelay(int close_delay);

  // Hides the detailed view for |item|.
  void HideDetailedView(SystemTrayItem* item);

  // Shows the notification view for |item|.
  void ShowNotificationView(SystemTrayItem* item);

  // Hides the notification view for |item|.
  void HideNotificationView(SystemTrayItem* item);

  // Updates the items when the login status of the system changes.
  void UpdateAfterLoginStatusChange(user::LoginStatus login_status);

  // Updates the items when the shelf alignment changes.
  void UpdateAfterShelfAlignmentChange(ShelfAlignment alignment);

  // Temporarily hides/unhides the notification bubble.
  void SetHideNotifications(bool hidden);

  // Returns true if there is a system bubble (already visible or in the process
  // of being created).
  bool HasSystemBubble() const;

  // Returns true if any bubble is visible.
  bool IsAnyBubbleVisible() const;

  // Returns true if the mouse is inside the notification bubble.
  bool IsMouseInNotificationBubble() const;

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
  LocaleObserver* locale_observer() const {
    return locale_observer_;
  }
  NetworkObserver* network_observer() const {
    return network_observer_;
  }
  ObserverList<PowerStatusObserver>& power_status_observers() {
    return power_status_observers_;
  }
  SmsObserver* sms_observer() const {
    return sms_observer_;
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

  // Overridden from TrayBackgroundView.
  virtual void Initialize() OVERRIDE;
  virtual void SetShelfAlignment(ShelfAlignment alignment) OVERRIDE;
  virtual void AnchorUpdated() OVERRIDE;
  virtual string16 GetAccessibleName() OVERRIDE;

 private:
  friend class internal::SystemTrayBubble;
  friend class internal::TrayGestureHandler;

  // Resets |bubble_| and clears any related state.
  void DestroyBubble();

  // Called when the widget associated with |bubble| closes. |bubble| should
  // always == |bubble_|. This triggers destroying |bubble_| and hiding the
  // launcher if necessary.
  void RemoveBubble(internal::SystemTrayBubble* bubble);

  const ScopedVector<SystemTrayItem>& items() const { return items_; }

  // Calculates the x-offset for the item in the tray. Returns -1 if its tray
  // item view is not visible.
  int GetTrayXOffset(SystemTrayItem* item) const;

  // Shows the default view and its arrow position is shifted by |x_offset|.
  void ShowDefaultViewWithOffset(BubbleCreationType creation_type,
                                 int x_offset);

  // Constructs or re-constructs |bubble_| and populates it with |items|.
  void ShowItems(const std::vector<SystemTrayItem*>& items,
                 bool details,
                 bool activate,
                 BubbleCreationType creation_type,
                 int x_offset);

  // Constructs or re-constructs |notification_bubble_| and populates it with
  // |notification_items_|, or destroys it if there are no notification items.
  void UpdateNotificationBubble();

  // Overridden from internal::ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE;

  // Owned items.
  ScopedVector<SystemTrayItem> items_;

  // Pointers to members of |items_|.
  SystemTrayItem* detailed_item_;
  std::vector<SystemTrayItem*> notification_items_;

  // Mappings of system tray item and it's view in the tray.
  std::map<SystemTrayItem*, views::View*> tray_item_map_;

  // These observers are not owned by the tray.
  AccessibilityObserver* accessibility_observer_;
  AudioObserver* audio_observer_;
  BluetoothObserver* bluetooth_observer_;
  BrightnessObserver* brightness_observer_;
  CapsLockObserver* caps_lock_observer_;
  ClockObserver* clock_observer_;
  DriveObserver* drive_observer_;
  IMEObserver* ime_observer_;
  LocaleObserver* locale_observer_;
  NetworkObserver* network_observer_;
  ObserverList<PowerStatusObserver> power_status_observers_;
  SmsObserver* sms_observer_;
  UpdateObserver* update_observer_;
  UserObserver* user_observer_;

  // Bubble for default and detailed views.
  scoped_ptr<internal::SystemTrayBubble> bubble_;

  // Bubble for notifications.
  scoped_ptr<internal::SystemTrayBubble> notification_bubble_;

  // Keep track of the default view height so that when we create detailed
  // views directly (e.g. from a notification) we know what height to use.
  int default_bubble_height_;

  // Set to true when system notifications should be hidden (e.g. web
  // notification bubble is visible).
  bool hide_notifications_;

  DISALLOW_COPY_AND_ASSIGN(SystemTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
