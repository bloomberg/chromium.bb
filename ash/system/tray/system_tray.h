// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_

#include "ash/ash_export.h"
#include "ash/system/tray/system_tray_bubble.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/user/login_status.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/view.h"

#include <map>
#include <vector>

namespace ash {
class SystemBubbleWrapper;
class SystemTrayDelegate;
class SystemTrayItem;
class TrayAccessibility;
class TrayDate;
class TrayUser;

// There are different methods for creating bubble views.
enum BubbleCreationType {
  BUBBLE_CREATE_NEW,    // Closes any existing bubble and creates a new one.
  BUBBLE_USE_EXISTING,  // Uses any existing bubble, or creates a new one.
};

class ASH_EXPORT SystemTray : public TrayBackgroundView,
                              public views::TrayBubbleView::Delegate {
 public:
  explicit SystemTray(StatusAreaWidget* status_area_widget);
  virtual ~SystemTray();

  // Calls TrayBackgroundView::Initialize(), creates the tray items, and
  // adds them to SystemTrayNotifier.
  void InitializeTrayItems(SystemTrayDelegate* delegate);

  // Adds a new item in the tray.
  void AddTrayItem(SystemTrayItem* item);

  // Removes an existing tray item.
  void RemoveTrayItem(SystemTrayItem* item);

  // Returns all tray items that has been added to system tray.
  const std::vector<SystemTrayItem*>& GetTrayItems() const;

  // Shows the default view of all items.
  void ShowDefaultView(BubbleCreationType creation_type);

  // Shows default view that ingnores outside clicks and activation loss.
  void ShowPersistentDefaultView();

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

  // Returns true if the shelf should be forced visible when auto-hidden.
  bool ShouldShowShelf() const;

  // Returns true if there is a system bubble (already visible or in the process
  // of being created).
  bool HasSystemBubble() const;

  // Returns true if there is a notification bubble.
  bool HasNotificationBubble() const;

  // Returns true if the system_bubble_ exists and is of type |type|.
  bool HasSystemBubbleType(SystemTrayBubble::BubbleType type);

  // Returns a pointer to the system bubble or NULL if none.
  SystemTrayBubble* GetSystemBubble();

  // Returns true if any bubble is visible.
  bool IsAnyBubbleVisible() const;

  // Returns true if the mouse is inside the notification bubble.
  bool IsMouseInNotificationBubble() const;

  // Closes system bubble and returns true if it did exist.
  bool CloseSystemBubble() const;

  // Returns view for help button if default view is shown. Returns NULL
  // otherwise.
  views::View* GetHelpButtonView() const;

  // Accessors for testing.

  // Returns true if the bubble exists.
  bool CloseNotificationBubbleForTest() const;

  // Overridden from TrayBackgroundView.
  virtual void SetShelfAlignment(ShelfAlignment alignment) OVERRIDE;
  virtual void AnchorUpdated() OVERRIDE;
  virtual base::string16 GetAccessibleNameForTray() OVERRIDE;
  virtual void BubbleResized(const views::TrayBubbleView* bubble_view) OVERRIDE;
  virtual void HideBubbleWithView(
      const views::TrayBubbleView* bubble_view) OVERRIDE;
  virtual bool ClickedOutsideBubble() OVERRIDE;

  // Overridden from message_center::TrayBubbleView::Delegate.
  virtual void BubbleViewDestroyed() OVERRIDE;
  virtual void OnMouseEnteredView() OVERRIDE;
  virtual void OnMouseExitedView() OVERRIDE;
  virtual base::string16 GetAccessibleNameForBubble() OVERRIDE;
  virtual gfx::Rect GetAnchorRect(
      views::Widget* anchor_widget,
      AnchorType anchor_type,
      AnchorAlignment anchor_alignment) const OVERRIDE;
  virtual void HideBubble(const views::TrayBubbleView* bubble_view) OVERRIDE;

  TrayAccessibility* GetTrayAccessibilityForTest() {
    return tray_accessibility_;
  }

  // Get the tray item view (or NULL) for a given |tray_item| in a unit test.
  views::View* GetTrayItemViewForTest(SystemTrayItem* tray_item);

  // Gets tray_date_ for browser tests.
  TrayDate* GetTrayDateForTesting() const;

 private:
  // Creates the default set of items for the sytem tray.
  void CreateItems(SystemTrayDelegate* delegate);

  // Resets |system_bubble_| and clears any related state.
  void DestroySystemBubble();

  // Resets |notification_bubble_| and clears any related state.
  void DestroyNotificationBubble();

  // Returns a string with the current time for accessibility on the status
  // tray bar.
  base::string16 GetAccessibleTimeString(const base::Time& now) const;

  // Calculates the x-offset for the item in the tray. Returns -1 if its tray
  // item view is not visible.
  int GetTrayXOffset(SystemTrayItem* item) const;

  // Shows the default view and its arrow position is shifted by |x_offset|.
  void ShowDefaultViewWithOffset(BubbleCreationType creation_type,
                                 int x_offset,
                                 bool persistent);

  // Constructs or re-constructs |system_bubble_| and populates it with |items|.
  // Specify |change_tray_status| to true if want to change the tray background
  // status.
  void ShowItems(const std::vector<SystemTrayItem*>& items,
                 bool details,
                 bool activate,
                 BubbleCreationType creation_type,
                 int x_offset,
                 bool persistent);

  // Constructs or re-constructs |notification_bubble_| and populates it with
  // |notification_items_|, or destroys it if there are no notification items.
  void UpdateNotificationBubble();

  // Checks the current status of the system tray and updates the web
  // notification tray according to the current status.
  void UpdateWebNotifications();

  // Deactivate the system tray in the shelf if it was active before.
  void CloseSystemBubbleAndDeactivateSystemTray();

  const ScopedVector<SystemTrayItem>& items() const { return items_; }

  // Overridden from ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE;

  // Owned items.
  ScopedVector<SystemTrayItem> items_;

  // Pointers to members of |items_|.
  SystemTrayItem* detailed_item_;
  std::vector<SystemTrayItem*> notification_items_;

  // Mappings of system tray item and it's view in the tray.
  std::map<SystemTrayItem*, views::View*> tray_item_map_;

  // Bubble for default and detailed views.
  scoped_ptr<SystemBubbleWrapper> system_bubble_;

  // Bubble for notifications.
  scoped_ptr<SystemBubbleWrapper> notification_bubble_;

  // Keep track of the default view height so that when we create detailed
  // views directly (e.g. from a notification) we know what height to use.
  int default_bubble_height_;

  // Set to true when system notifications should be hidden (e.g. web
  // notification bubble is visible).
  bool hide_notifications_;

  // This is true when the displayed system tray menu is a full tray menu,
  // otherwise a single line item menu like the volume slider is shown.
  // Note that the value is only valid when |system_bubble_| is true.
  bool full_system_tray_menu_;

  TrayAccessibility* tray_accessibility_;  // not owned
  TrayDate* tray_date_;

  DISALLOW_COPY_AND_ASSIGN(SystemTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
