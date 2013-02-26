// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell/panel_window.h"
#include "ash/shell_window_ids.h"
#include "ash/system/audio/tray_volume.h"
#include "ash/system/bluetooth/tray_bluetooth.h"
#include "ash/system/brightness/tray_brightness.h"
#include "ash/system/chromeos/tray_display.h"
#include "ash/system/date/tray_date.h"
#include "ash/system/drive/tray_drive.h"
#include "ash/system/ime/tray_ime.h"
#include "ash/system/locale/tray_locale.h"
#include "ash/system/logout_button/tray_logout_button.h"
#include "ash/system/monitor/tray_monitor.h"
#include "ash/system/power/power_supply_status.h"
#include "ash/system/power/tray_power.h"
#include "ash/system/session_length_limit/tray_session_length_limit.h"
#include "ash/system/settings/tray_settings.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/tray_caps_lock.h"
#include "ash/system/tray_update.h"
#include "ash/system/user/login_status.h"
#include "ash/system/user/tray_user.h"
#include "ash/wm/shelf_layout_manager.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

#if defined(OS_CHROMEOS)
#include "ash/system/chromeos/enterprise/tray_enterprise.h"
#include "ash/system/chromeos/network/tray_network.h"
#include "ash/system/chromeos/network/tray_sms.h"
#include "ash/system/chromeos/network/tray_vpn.h"
#endif

using views::TrayBubbleView;

namespace ash {

namespace internal {

// Class to initialize and manage the SystemTrayBubble and TrayBubbleWrapper
// instances for a bubble.

class SystemBubbleWrapper {
 public:
  // Takes ownership of |bubble|.
  explicit SystemBubbleWrapper(internal::SystemTrayBubble* bubble)
      : bubble_(bubble) {
  }

  // Initializes the bubble view and creates |bubble_wrapper_|.
  void InitView(TrayBackgroundView* tray,
                views::View* anchor,
                TrayBubbleView::InitParams* init_params) {
    user::LoginStatus login_status =
        Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus();
    bubble_->InitView(anchor, login_status, init_params);
    bubble_wrapper_.reset(
        new internal::TrayBubbleWrapper(tray, bubble_->bubble_view()));
  }

  // Convenience accessors:
  SystemTrayBubble* bubble() const { return bubble_.get(); }
  SystemTrayBubble::BubbleType bubble_type() const {
    return bubble_->bubble_type();
  }
  TrayBubbleView* bubble_view() const { return bubble_->bubble_view(); }

 private:
  scoped_ptr<internal::SystemTrayBubble> bubble_;
  scoped_ptr<internal::TrayBubbleWrapper> bubble_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(SystemBubbleWrapper);
};

}  // namespace internal

// SystemTray

using internal::SystemTrayBubble;

SystemTray::SystemTray(internal::StatusAreaWidget* status_area_widget)
    : internal::TrayBackgroundView(status_area_widget),
      items_(),
      default_bubble_height_(0),
      hide_notifications_(false) {
  SetContentsBackground();
}

SystemTray::~SystemTray() {
  // Destroy any child views that might have back pointers before ~View().
  system_bubble_.reset();
  notification_bubble_.reset();
  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
       it != items_.end();
       ++it) {
    (*it)->DestroyTrayView();
  }
}

void SystemTray::InitializeTrayItems(SystemTrayDelegate* delegate) {
  internal::TrayBackgroundView::Initialize();
  CreateItems(delegate);
}

void SystemTray::CreateItems(SystemTrayDelegate* delegate) {
  AddTrayItem(new internal::TraySessionLengthLimit(this));
  AddTrayItem(new internal::TrayLogoutButton(this));
  AddTrayItem(new internal::TrayUser(this));
#if defined(OS_CHROMEOS)
  AddTrayItem(new internal::TrayEnterprise(this));
#endif
  AddTrayItem(new internal::TrayIME(this));
  tray_accessibility_ = new internal::TrayAccessibility(this);
  AddTrayItem(tray_accessibility_);
  AddTrayItem(new internal::TrayPower(this));
#if defined(OS_CHROMEOS)
  AddTrayItem(new internal::TrayNetwork(this));
  AddTrayItem(new internal::TrayVPN(this));
  AddTrayItem(new internal::TraySms(this));
#endif
  AddTrayItem(new internal::TrayBluetooth(this));
  AddTrayItem(new internal::TrayDrive(this));
  AddTrayItem(new internal::TrayLocale(this));
#if defined(OS_CHROMEOS)
  AddTrayItem(new internal::TrayDisplay(this));
#endif
  AddTrayItem(new internal::TrayVolume(this));
  AddTrayItem(new internal::TrayBrightness(this));
  AddTrayItem(new internal::TrayCapsLock(this));
  AddTrayItem(new internal::TraySettings(this));
  AddTrayItem(new internal::TrayUpdate(this));
  AddTrayItem(new internal::TrayDate(this));

#if defined(OS_LINUX)
  // Add memory monitor if enabled.
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(ash::switches::kAshEnableMemoryMonitor))
    AddTrayItem(new internal::TrayMonitor(this));
#endif

  SetVisible(ash::Shell::GetInstance()->system_tray_delegate()->
      GetTrayVisibilityOnStartup());
}

void SystemTray::AddTrayItem(SystemTrayItem* item) {
  items_.push_back(item);

  SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
  views::View* tray_item = item->CreateTrayView(delegate->GetUserLoginStatus());
  item->UpdateAfterShelfAlignmentChange(shelf_alignment());

  if (tray_item) {
    tray_container()->AddChildViewAt(tray_item, 0);
    PreferredSizeChanged();
    tray_item_map_[item] = tray_item;
  }
}

void SystemTray::RemoveTrayItem(SystemTrayItem* item) {
  NOTIMPLEMENTED();
}

void SystemTray::ShowDefaultView(BubbleCreationType creation_type) {
  ShowDefaultViewWithOffset(creation_type,
                            TrayBubbleView::InitParams::kArrowDefaultOffset);
}

void SystemTray::ShowDetailedView(SystemTrayItem* item,
                                  int close_delay,
                                  bool activate,
                                  BubbleCreationType creation_type) {
  std::vector<SystemTrayItem*> items;
  items.push_back(item);
  ShowItems(items, true, activate, creation_type, GetTrayXOffset(item));
  if (system_bubble_.get())
    system_bubble_->bubble()->StartAutoCloseTimer(close_delay);
}

void SystemTray::SetDetailedViewCloseDelay(int close_delay) {
  if (HasSystemBubbleType(SystemTrayBubble::BUBBLE_TYPE_DETAILED))
    system_bubble_->bubble()->StartAutoCloseTimer(close_delay);
}

void SystemTray::HideDetailedView(SystemTrayItem* item) {
  if (item != detailed_item_)
    return;
  DestroySystemBubble();
  UpdateNotificationBubble();
}

void SystemTray::ShowNotificationView(SystemTrayItem* item) {
  if (std::find(notification_items_.begin(), notification_items_.end(), item)
      != notification_items_.end())
    return;
  notification_items_.push_back(item);
  UpdateNotificationBubble();
}

void SystemTray::HideNotificationView(SystemTrayItem* item) {
  std::vector<SystemTrayItem*>::iterator found_iter =
      std::find(notification_items_.begin(), notification_items_.end(), item);
  if (found_iter == notification_items_.end())
    return;
  notification_items_.erase(found_iter);
  // Only update the notification bubble if visible (i.e. don't create one).
  if (notification_bubble_.get())
    UpdateNotificationBubble();
}

void SystemTray::UpdateAfterLoginStatusChange(user::LoginStatus login_status) {
  DestroySystemBubble();

  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
      it != items_.end();
      ++it) {
    (*it)->UpdateAfterLoginStatusChange(login_status);
  }

  SetVisible(true);
  PreferredSizeChanged();
}

void SystemTray::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
      it != items_.end();
      ++it) {
    (*it)->UpdateAfterShelfAlignmentChange(alignment);
  }
}

void SystemTray::SetHideNotifications(bool hide_notifications) {
  if (notification_bubble_.get())
    notification_bubble_->bubble()->SetVisible(!hide_notifications);
  hide_notifications_ = hide_notifications;
}

bool SystemTray::ShouldShowLauncher() const {
  return system_bubble_.get() && system_bubble_->bubble()->ShouldShowLauncher();
}

bool SystemTray::HasSystemBubble() const {
  return system_bubble_.get() != NULL;
}

internal::SystemTrayBubble* SystemTray::GetSystemBubble() {
  if (!system_bubble_.get())
    return NULL;
  return system_bubble_->bubble();
}

bool SystemTray::IsAnyBubbleVisible() const {
  return ((system_bubble_.get() &&
           system_bubble_->bubble()->IsVisible()) ||
          (notification_bubble_.get() &&
           notification_bubble_->bubble()->IsVisible()));
}

bool SystemTray::IsMouseInNotificationBubble() const {
  if (!notification_bubble_.get())
    return false;
  return notification_bubble_->bubble_view()->GetBoundsInScreen().Contains(
      Shell::GetScreen()->GetCursorScreenPoint());
}

bool SystemTray::CloseBubbleForTest() const {
  if (!system_bubble_.get())
    return false;
  system_bubble_->bubble()->Close();
  return true;
}

// Private methods.

bool SystemTray::HasSystemBubbleType(SystemTrayBubble::BubbleType type) {
  DCHECK(type != SystemTrayBubble::BUBBLE_TYPE_NOTIFICATION);
  return system_bubble_.get() && system_bubble_->bubble_type() == type;
}

void SystemTray::DestroySystemBubble() {
  system_bubble_.reset();
  detailed_item_ = NULL;
}

void SystemTray::DestroyNotificationBubble() {
  notification_bubble_.reset();
  status_area_widget()->SetHideWebNotifications(false);
}

int SystemTray::GetTrayXOffset(SystemTrayItem* item) const {
  // Don't attempt to align the arrow if the shelf is on the left or right.
  if (shelf_alignment() != SHELF_ALIGNMENT_BOTTOM &&
      shelf_alignment() != SHELF_ALIGNMENT_TOP)
    return TrayBubbleView::InitParams::kArrowDefaultOffset;

  std::map<SystemTrayItem*, views::View*>::const_iterator it =
      tray_item_map_.find(item);
  if (it == tray_item_map_.end())
    return TrayBubbleView::InitParams::kArrowDefaultOffset;

  const views::View* item_view = it->second;
  if (item_view->bounds().IsEmpty()) {
    // The bounds of item could be still empty if it does not have a visible
    // tray view. In that case, use the default (minimum) offset.
    return TrayBubbleView::InitParams::kArrowDefaultOffset;
  }

  gfx::Point point(item_view->width() / 2, 0);
  ConvertPointToWidget(item_view, &point);
  return point.x();
}

void SystemTray::ShowDefaultViewWithOffset(BubbleCreationType creation_type,
                                           int arrow_offset) {
  ShowItems(items_.get(), false, true, creation_type, arrow_offset);
}

void SystemTray::ShowItems(const std::vector<SystemTrayItem*>& items,
                           bool detailed,
                           bool can_activate,
                           BubbleCreationType creation_type,
                           int arrow_offset) {
  // Destroy any existing bubble and create a new one.
  SystemTrayBubble::BubbleType bubble_type = detailed ?
      SystemTrayBubble::BUBBLE_TYPE_DETAILED :
      SystemTrayBubble::BUBBLE_TYPE_DEFAULT;

  // Destroy the notification bubble here so that it doesn't get rebuilt
  // while we add items to the main bubble_ (e.g. in HideNotificationView).
  notification_bubble_.reset();

  if (system_bubble_.get() && creation_type == BUBBLE_USE_EXISTING) {
    system_bubble_->bubble()->UpdateView(items, bubble_type);
  } else {
    TrayBubbleView::InitParams init_params(TrayBubbleView::ANCHOR_TYPE_TRAY,
                                           GetAnchorAlignment(),
                                           kTrayPopupMinWidth,
                                           kTrayPopupMaxWidth);
    init_params.can_activate = can_activate;
    if (detailed) {
      // This is the case where a volume control or brightness control bubble
      // is created.
      init_params.max_height = default_bubble_height_;
      init_params.arrow_color = kBackgroundColor;
    } else {
      init_params.arrow_color = kHeaderBackgroundColor;
    }
    init_params.arrow_offset = arrow_offset;
    // For Volume and Brightness we don't want to show an arrow when
    // they are shown in a bubble by themselves.
    init_params.arrow_paint_type = views::BubbleBorder::PAINT_NORMAL;
    if (items.size() == 1 && items[0]->ShouldHideArrow())
      init_params.arrow_paint_type = views::BubbleBorder::PAINT_TRANSPARENT;
    SystemTrayBubble* bubble = new SystemTrayBubble(this, items, bubble_type);
    system_bubble_.reset(new internal::SystemBubbleWrapper(bubble));
    system_bubble_->InitView(this, tray_container(), &init_params);
  }
  // Save height of default view for creating detailed views directly.
  if (!detailed)
    default_bubble_height_ = system_bubble_->bubble_view()->height();

  if (detailed && items.size() > 0)
    detailed_item_ = items[0];
  else
    detailed_item_ = NULL;

  UpdateNotificationBubble();  // State changed, re-create notifications.
  status_area_widget()->SetHideWebNotifications(true);
  GetShelfLayoutManager()->UpdateAutoHideState();
}

void SystemTray::UpdateNotificationBubble() {
  // Only show the notification buble if we have notifications.
  if (notification_items_.empty()) {
    DestroyNotificationBubble();
    return;
  }
  // Destroy the existing bubble before constructing a new one.
  notification_bubble_.reset();
  SystemTrayBubble* notification_bubble;
  notification_bubble = new SystemTrayBubble(
      this, notification_items_, SystemTrayBubble::BUBBLE_TYPE_NOTIFICATION);
  views::View* anchor;
  TrayBubbleView::AnchorType anchor_type;
  if (system_bubble_.get()) {
    anchor = system_bubble_->bubble_view();
    anchor_type = TrayBubbleView::ANCHOR_TYPE_BUBBLE;
  } else {
    anchor = tray_container();
    anchor_type = TrayBubbleView::ANCHOR_TYPE_TRAY;
  }
  TrayBubbleView::InitParams init_params(anchor_type,
                                         GetAnchorAlignment(),
                                         kTrayPopupMinWidth,
                                         kTrayPopupMaxWidth);
  init_params.arrow_color = kBackgroundColor;
  init_params.arrow_offset = GetTrayXOffset(notification_items_[0]);
  notification_bubble_.reset(
      new internal::SystemBubbleWrapper(notification_bubble));
  notification_bubble_->InitView(this, anchor, &init_params);

  if (notification_bubble->bubble_view()->child_count() == 0) {
    // It is possible that none of the items generated actual notifications.
    DestroyNotificationBubble();
    return;
  }
  if (hide_notifications_)
    notification_bubble->SetVisible(false);
  else
    status_area_widget()->SetHideWebNotifications(true);
}

void SystemTray::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment == shelf_alignment())
    return;
  internal::TrayBackgroundView::SetShelfAlignment(alignment);
  UpdateAfterShelfAlignmentChange(alignment);
  // Destroy any existing bubble so that it is rebuilt correctly.
  system_bubble_.reset();
  // Rebuild any notification bubble.
  if (notification_bubble_.get()) {
    notification_bubble_.reset();
    UpdateNotificationBubble();
  }
}

void SystemTray::AnchorUpdated() {
  if (notification_bubble_.get()) {
    notification_bubble_->bubble_view()->UpdateBubble();
    // Ensure that the notification buble is above the launcher/status area.
    notification_bubble_->bubble_view()->GetWidget()->StackAtTop();
    UpdateBubbleViewArrow(notification_bubble_->bubble_view());
  }
  if (system_bubble_.get()) {
    system_bubble_->bubble_view()->UpdateBubble();
    UpdateBubbleViewArrow(system_bubble_->bubble_view());
  }
}

string16 SystemTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBLE_NAME);
}

void SystemTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (system_bubble_.get() && bubble_view == system_bubble_->bubble_view()) {
    DestroySystemBubble();
    UpdateNotificationBubble();  // State changed, re-create notifications.
    GetShelfLayoutManager()->UpdateAutoHideState();
  } else if (notification_bubble_.get() &&
             bubble_view == notification_bubble_->bubble_view()) {
    DestroyNotificationBubble();
  }
}

bool SystemTray::ClickedOutsideBubble() {
  if (!system_bubble_.get())
    return false;
  HideBubbleWithView(system_bubble_->bubble_view());
  return true;
}

void SystemTray::BubbleViewDestroyed() {
  if (system_bubble_.get()) {
    system_bubble_->bubble()->DestroyItemViews();
    system_bubble_->bubble()->BubbleViewDestroyed();
  }
}

void SystemTray::OnMouseEnteredView() {
  if (system_bubble_.get())
    system_bubble_->bubble()->StopAutoCloseTimer();
}

void SystemTray::OnMouseExitedView() {
  if (system_bubble_.get())
    system_bubble_->bubble()->RestartAutoCloseTimer();
}

string16 SystemTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

gfx::Rect SystemTray::GetAnchorRect(
    views::Widget* anchor_widget,
    TrayBubbleView::AnchorType anchor_type,
    TrayBubbleView::AnchorAlignment anchor_alignment) {
  return GetBubbleAnchorRect(anchor_widget, anchor_type, anchor_alignment);
}

void SystemTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

bool SystemTray::PerformAction(const ui::Event& event) {
  // If we're already showing the default view, hide it; otherwise, show it
  // (and hide any popup that's currently shown).
  if (HasSystemBubbleType(SystemTrayBubble::BUBBLE_TYPE_DEFAULT)) {
    system_bubble_->bubble()->Close();
  } else {
    int arrow_offset = TrayBubbleView::InitParams::kArrowDefaultOffset;
    if (event.IsMouseEvent() || event.type() == ui::ET_GESTURE_TAP) {
      const ui::LocatedEvent& located_event =
          static_cast<const ui::LocatedEvent&>(event);
      if (shelf_alignment() == SHELF_ALIGNMENT_BOTTOM ||
          shelf_alignment() == SHELF_ALIGNMENT_TOP) {
        gfx::Point point(located_event.x(), 0);
        ConvertPointToWidget(this, &point);
        arrow_offset = point.x();
      }
    }
    ShowDefaultViewWithOffset(BUBBLE_CREATE_NEW, arrow_offset);
  }
  return true;
}

}  // namespace ash
