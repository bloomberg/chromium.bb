// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_

#include <map>
#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/tray/system_tray_bubble.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/macros.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/view.h"

namespace ash {

class KeyEventWatcher;
enum class LoginStatus;
class ScreenTrayItem;
class SystemBubbleWrapper;
class SystemTrayDelegate;
class SystemTrayItem;
class TrayAccessibility;
class TrayAudio;
class TrayCast;
class TrayEnterprise;
class TrayNetwork;
class TrayNightLight;
class TrayScale;
class TraySupervisedUser;
class TraySystemInfo;
class TrayTiles;
class TrayUpdate;
class WebNotificationTray;

// There are different methods for creating bubble views.
enum BubbleCreationType {
  BUBBLE_CREATE_NEW,    // Closes any existing bubble and creates a new one.
  BUBBLE_USE_EXISTING,  // Uses any existing bubble, or creates a new one.
};

class ASH_EXPORT SystemTray : public TrayBackgroundView,
                              public views::TrayBubbleView::Delegate {
 public:
  explicit SystemTray(Shelf* shelf);
  ~SystemTray() override;

  TrayUpdate* tray_update() { return tray_update_; }

  TrayNightLight* tray_night_light() { return tray_night_light_; }

  // Calls TrayBackgroundView::Initialize(), creates the tray items, and
  // adds them to SystemTrayNotifier.
  void InitializeTrayItems(SystemTrayDelegate* delegate,
                           WebNotificationTray* web_notification_tray);

  // Resets internal pointers. This has to be called before deletion.
  void Shutdown();

  // Adds a new item in the tray.
  void AddTrayItem(std::unique_ptr<SystemTrayItem> item);

  // Returns all tray items that has been added to system tray.
  std::vector<SystemTrayItem*> GetTrayItems() const;

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

  // Hides the detailed view for |item|. If |animate| is false, disable
  // the hiding animation for hiding |item|.
  void HideDetailedView(SystemTrayItem* item, bool animate);

  // Updates the items when the login status of the system changes.
  void UpdateAfterLoginStatusChange(LoginStatus login_status);

  // Updates the items when the shelf alignment changes.
  void UpdateItemsAfterShelfAlignmentChange();

  // Returns true if the shelf should be forced visible when auto-hidden.
  bool ShouldShowShelf() const;

  // Returns true if there is a system bubble (already visible or in the process
  // of being created).
  bool HasSystemBubble() const;

  // Returns true if the system_bubble_ exists and is of type |type|.
  bool HasSystemBubbleType(SystemTrayBubble::BubbleType type);

  // Returns a pointer to the system bubble or NULL if none.
  SystemTrayBubble* GetSystemBubble();

  // Returns true if system bubble is visible.
  bool IsSystemBubbleVisible() const;

  // Closes system bubble and returns true if it did exist.
  bool CloseSystemBubble() const;

  // Returns view for help button if default view is shown. Returns NULL
  // otherwise.
  views::View* GetHelpButtonView() const;

  // Returns TrayAudio object if present or null otherwise.
  TrayAudio* GetTrayAudio() const;

  // Overridden from TrayBackgroundView.
  void UpdateAfterShelfAlignmentChange() override;
  void AnchorUpdated() override;
  base::string16 GetAccessibleNameForTray() override;
  void BubbleResized(const views::TrayBubbleView* bubble_view) override;
  void HideBubbleWithView(const views::TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;

  // views::TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;
  void OnMouseEnteredView() override;
  void OnMouseExitedView() override;
  base::string16 GetAccessibleNameForBubble() override;
  void OnBeforeBubbleWidgetInit(
      views::Widget* anchor_widget,
      views::Widget* bubble_widget,
      views::Widget::InitParams* params) const override;
  void HideBubble(const views::TrayBubbleView* bubble_view) override;

  ScreenTrayItem* GetScreenShareItem() { return screen_share_tray_item_; }
  ScreenTrayItem* GetScreenCaptureItem() { return screen_capture_tray_item_; }

  TrayAccessibility* GetTrayAccessibilityForTest() {
    return tray_accessibility_;
  }

  // TODO(jamescook): Add a SystemTrayTestApi instead of these methods.
  TrayCast* GetTrayCastForTesting() const;
  TrayEnterprise* GetTrayEnterpriseForTesting() const;
  TrayNetwork* GetTrayNetworkForTesting() const;
  TraySupervisedUser* GetTraySupervisedUserForTesting() const;
  TraySystemInfo* GetTraySystemInfoForTesting() const;
  TrayTiles* GetTrayTilesForTesting() const;

  // Activates the system tray bubble.
  void ActivateBubble();

 private:
  class ActivationObserver;

  // Closes the bubble. Used to bind as a KeyEventWatcher::KeyEventCallback.
  void CloseBubble(const ui::KeyEvent& key_event);

  // Activates the bubble and starts key navigation with the |key_event|.
  void ActivateAndStartNavigation(const ui::KeyEvent& key_event);

  // Creates the key event watcher. See |ShowItems()| for why key events are
  // observed.
  void CreateKeyEventWatcher();

  // Creates the default set of items for the sytem tray.
  void CreateItems(SystemTrayDelegate* delegate);

  // Resets |system_bubble_| and clears any related state.
  void DestroySystemBubble();

  // Returns a string with the current time for accessibility on the status
  // tray bar.
  base::string16 GetAccessibleTimeString(const base::Time& now) const;

  // Constructs or re-constructs |system_bubble_| and populates it with |items|.
  // Specify |change_tray_status| to true if want to change the tray background
  // status. The bubble will be opened in inactive state. If |can_activate| is
  // true, the bubble will be activated by one of following means.
  // * When alt/alt-tab acclerator is used to start navigation.
  // * When the bubble is opened by accelerator.
  // * When the tray item is set to be focused.
  void ShowItems(const std::vector<SystemTrayItem*>& items,
                 bool details,
                 bool can_activate,
                 BubbleCreationType creation_type,
                 bool persistent);

  // Checks the current status of the system tray and updates the web
  // notification tray according to the current status.
  void UpdateWebNotifications();

  // Deactivate the system tray in the shelf if it was active before.
  void CloseSystemBubbleAndDeactivateSystemTray();

  // Records UMA metrics for the number of user-visible rows in the system menu
  // and the percentage of the work area height covered by the system menu.
  void RecordSystemMenuMetrics();

  // Overridden from ActionableView.
  bool PerformAction(const ui::Event& event) override;

  // The web notification tray view that appears adjacent to this view.
  WebNotificationTray* web_notification_tray_ = nullptr;

  // Items.
  std::vector<std::unique_ptr<SystemTrayItem>> items_;

  // Pointers to members of |items_|.
  SystemTrayItem* detailed_item_ = nullptr;

  // Bubble for default and detailed views.
  std::unique_ptr<SystemBubbleWrapper> system_bubble_;

  // Keep track of the default view height so that when we create detailed
  // views directly (e.g. from a notification) we know what height to use.
  int default_bubble_height_ = 0;

  // This is true when the displayed system tray menu is a full tray menu,
  // otherwise a single line item menu like the volume slider is shown.
  // Note that the value is only valid when |system_bubble_| is true.
  bool full_system_tray_menu_ = false;

  // These objects are not owned by this class.
  TrayAccessibility* tray_accessibility_ = nullptr;
  TrayAudio* tray_audio_ = nullptr;
  TrayCast* tray_cast_ = nullptr;
  TrayEnterprise* tray_enterprise_ = nullptr;
  TrayNetwork* tray_network_ = nullptr;
  TrayTiles* tray_tiles_ = nullptr;
  TrayScale* tray_scale_ = nullptr;
  TraySupervisedUser* tray_supervised_user_ = nullptr;
  TraySystemInfo* tray_system_info_ = nullptr;
  TrayUpdate* tray_update_ = nullptr;
  TrayNightLight* tray_night_light_ = nullptr;

  // A reference to the Screen share and capture item.
  ScreenTrayItem* screen_capture_tray_item_ = nullptr;  // not owned
  ScreenTrayItem* screen_share_tray_item_ = nullptr;    // not owned

  std::unique_ptr<KeyEventWatcher> key_event_watcher_;

  std::unique_ptr<ActivationObserver> activation_observer_;

  DISALLOW_COPY_AND_ASSIGN(SystemTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
