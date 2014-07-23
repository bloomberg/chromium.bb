// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray.h"

#include "ash/ash_switches.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell/panel_window.h"
#include "ash/shell_window_ids.h"
#include "ash/system/audio/tray_audio.h"
#include "ash/system/bluetooth/tray_bluetooth.h"
#include "ash/system/date/tray_date.h"
#include "ash/system/drive/tray_drive.h"
#include "ash/system/ime/tray_ime.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/tray_update.h"
#include "ash/system/user/login_status.h"
#include "ash/system/user/tray_user.h"
#include "ash/system/user/tray_user_separator.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "grit/ash_strings.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

#if defined(OS_CHROMEOS)
#include "ash/system/chromeos/audio/tray_audio_chromeos.h"
#include "ash/system/chromeos/brightness/tray_brightness.h"
#include "ash/system/chromeos/enterprise/tray_enterprise.h"
#include "ash/system/chromeos/network/tray_network.h"
#include "ash/system/chromeos/network/tray_sms.h"
#include "ash/system/chromeos/network/tray_vpn.h"
#include "ash/system/chromeos/power/power_status.h"
#include "ash/system/chromeos/power/tray_power.h"
#include "ash/system/chromeos/rotation/tray_rotation_lock.h"
#include "ash/system/chromeos/screen_security/screen_capture_tray_item.h"
#include "ash/system/chromeos/screen_security/screen_share_tray_item.h"
#include "ash/system/chromeos/session/tray_session_length_limit.h"
#include "ash/system/chromeos/settings/tray_settings.h"
#include "ash/system/chromeos/supervised/tray_supervised_user.h"
#include "ash/system/chromeos/tray_caps_lock.h"
#include "ash/system/chromeos/tray_display.h"
#include "ash/system/chromeos/tray_tracing.h"
#include "ash/system/tray/media_security/multi_profile_media_tray_item.h"
#include "ui/message_center/message_center.h"
#elif defined(OS_WIN)
#include "ash/system/win/audio/tray_audio_win.h"
#include "media/audio/win/core_audio_util_win.h"
#endif

using views::TrayBubbleView;

namespace ash {

// The minimum width of the system tray menu width.
const int kMinimumSystemTrayMenuWidth = 300;

// Class to initialize and manage the SystemTrayBubble and TrayBubbleWrapper
// instances for a bubble.

class SystemBubbleWrapper {
 public:
  // Takes ownership of |bubble|.
  explicit SystemBubbleWrapper(SystemTrayBubble* bubble)
      : bubble_(bubble), is_persistent_(false) {}

  // Initializes the bubble view and creates |bubble_wrapper_|.
  void InitView(TrayBackgroundView* tray,
                views::View* anchor,
                TrayBubbleView::InitParams* init_params,
                bool is_persistent) {
    DCHECK(anchor);
    user::LoginStatus login_status =
        Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus();
    bubble_->InitView(anchor, login_status, init_params);
    bubble_wrapper_.reset(new TrayBubbleWrapper(tray, bubble_->bubble_view()));
    // The system bubble should not have an arrow.
    bubble_->bubble_view()->SetArrowPaintType(
        views::BubbleBorder::PAINT_NONE);
    is_persistent_ = is_persistent;

    // If ChromeVox is enabled, focus the default item if no item is focused.
    if (Shell::GetInstance()->accessibility_delegate()->
        IsSpokenFeedbackEnabled()) {
      bubble_->FocusDefaultIfNeeded();
    }
  }

  // Convenience accessors:
  SystemTrayBubble* bubble() const { return bubble_.get(); }
  SystemTrayBubble::BubbleType bubble_type() const {
    return bubble_->bubble_type();
  }
  TrayBubbleView* bubble_view() const { return bubble_->bubble_view(); }
  bool is_persistent() const { return is_persistent_; }

 private:
  scoped_ptr<SystemTrayBubble> bubble_;
  scoped_ptr<TrayBubbleWrapper> bubble_wrapper_;
  bool is_persistent_;

  DISALLOW_COPY_AND_ASSIGN(SystemBubbleWrapper);
};


// SystemTray

SystemTray::SystemTray(StatusAreaWidget* status_area_widget)
    : TrayBackgroundView(status_area_widget),
      items_(),
      default_bubble_height_(0),
      hide_notifications_(false),
      full_system_tray_menu_(false),
      tray_accessibility_(NULL),
      tray_date_(NULL) {
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
  TrayBackgroundView::Initialize();
  CreateItems(delegate);
}

void SystemTray::CreateItems(SystemTrayDelegate* delegate) {
#if !defined(OS_WIN)
  // Create user items for each possible user.
  ash::Shell* shell = ash::Shell::GetInstance();
  int maximum_user_profiles =
          shell->session_state_delegate()->GetMaximumNumberOfLoggedInUsers();
  for (int i = 0; i < maximum_user_profiles; i++)
    AddTrayItem(new TrayUser(this, i));

  if (maximum_user_profiles > 1) {
    // Add a special double line separator between users and the rest of the
    // menu if more then one user is logged in.
    AddTrayItem(new TrayUserSeparator(this));
  }
#endif

  tray_accessibility_ = new TrayAccessibility(this);
  tray_date_ = new TrayDate(this);

#if defined(OS_CHROMEOS)
  AddTrayItem(new TraySessionLengthLimit(this));
  AddTrayItem(new TrayEnterprise(this));
  AddTrayItem(new TraySupervisedUser(this));
  AddTrayItem(new TrayIME(this));
  AddTrayItem(tray_accessibility_);
  AddTrayItem(new TrayTracing(this));
  AddTrayItem(new TrayPower(this, message_center::MessageCenter::Get()));
  AddTrayItem(new TrayNetwork(this));
  AddTrayItem(new TrayVPN(this));
  AddTrayItem(new TraySms(this));
  AddTrayItem(new TrayBluetooth(this));
  AddTrayItem(new TrayDrive(this));
  AddTrayItem(new TrayDisplay(this));
  AddTrayItem(new ScreenCaptureTrayItem(this));
  AddTrayItem(new ScreenShareTrayItem(this));
  AddTrayItem(new MultiProfileMediaTrayItem(this));
  AddTrayItem(new TrayAudioChromeOs(this));
  AddTrayItem(new TrayBrightness(this));
  AddTrayItem(new TrayCapsLock(this));
  AddTrayItem(new TraySettings(this));
  AddTrayItem(new TrayUpdate(this));
  AddTrayItem(new TrayRotationLock(this));
  AddTrayItem(tray_date_);
#elif defined(OS_WIN)
  AddTrayItem(tray_accessibility_);
  if (media::CoreAudioUtil::IsSupported())
    AddTrayItem(new TrayAudioWin(this));
  AddTrayItem(new TrayUpdate(this));
  AddTrayItem(tray_date_);
#elif defined(OS_LINUX)
  AddTrayItem(new TrayIME(this));
  AddTrayItem(tray_accessibility_);
  AddTrayItem(new TrayBluetooth(this));
  AddTrayItem(new TrayDrive(this));
  AddTrayItem(new TrayUpdate(this));
  AddTrayItem(tray_date_);
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

const std::vector<SystemTrayItem*>& SystemTray::GetTrayItems() const {
  return items_.get();
}

void SystemTray::ShowDefaultView(BubbleCreationType creation_type) {
  ShowDefaultViewWithOffset(
      creation_type,
      TrayBubbleView::InitParams::kArrowDefaultOffset,
      false);
}

void SystemTray::ShowPersistentDefaultView() {
  ShowItems(items_.get(),
            false,
            false,
            BUBBLE_CREATE_NEW,
            TrayBubbleView::InitParams::kArrowDefaultOffset,
            true);
}

void SystemTray::ShowDetailedView(SystemTrayItem* item,
                                  int close_delay,
                                  bool activate,
                                  BubbleCreationType creation_type) {
  std::vector<SystemTrayItem*> items;
  // The detailed view with timeout means a UI to show the current system state,
  // like the audio level or brightness. Such UI should behave as persistent and
  // keep its own logic for the appearance.
  bool persistent = (
      !activate && close_delay > 0 && creation_type == BUBBLE_CREATE_NEW);
  items.push_back(item);
  ShowItems(
      items, true, activate, creation_type, GetTrayXOffset(item), persistent);
  if (system_bubble_)
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
  if (notification_bubble_)
    UpdateNotificationBubble();
}

void SystemTray::UpdateAfterLoginStatusChange(user::LoginStatus login_status) {
  DestroySystemBubble();
  UpdateNotificationBubble();

  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
      it != items_.end();
      ++it) {
    (*it)->UpdateAfterLoginStatusChange(login_status);
  }

  // Items default to SHELF_ALIGNMENT_BOTTOM. Update them if the initial
  // position of the shelf differs.
  if (shelf_alignment() != SHELF_ALIGNMENT_BOTTOM)
    UpdateAfterShelfAlignmentChange(shelf_alignment());

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
  if (notification_bubble_)
    notification_bubble_->bubble()->SetVisible(!hide_notifications);
  hide_notifications_ = hide_notifications;
}

bool SystemTray::ShouldShowShelf() const {
  return system_bubble_.get() && system_bubble_->bubble()->ShouldShowShelf();
}

bool SystemTray::HasSystemBubble() const {
  return system_bubble_.get() != NULL;
}

bool SystemTray::HasNotificationBubble() const {
  return notification_bubble_.get() != NULL;
}

SystemTrayBubble* SystemTray::GetSystemBubble() {
  if (!system_bubble_)
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
  if (!notification_bubble_)
    return false;
  return notification_bubble_->bubble_view()->GetBoundsInScreen().Contains(
      Shell::GetScreen()->GetCursorScreenPoint());
}

bool SystemTray::CloseSystemBubble() const {
  if (!system_bubble_)
    return false;
  system_bubble_->bubble()->Close();
  return true;
}

views::View* SystemTray::GetHelpButtonView() const {
  return tray_date_->GetHelpButtonView();
}

bool SystemTray::CloseNotificationBubbleForTest() const {
  if (!notification_bubble_)
    return false;
  notification_bubble_->bubble()->Close();
  return true;
}

// Private methods.

bool SystemTray::HasSystemBubbleType(SystemTrayBubble::BubbleType type) {
  DCHECK(type != SystemTrayBubble::BUBBLE_TYPE_NOTIFICATION);
  return system_bubble_.get() && system_bubble_->bubble_type() == type;
}

void SystemTray::DestroySystemBubble() {
  CloseSystemBubbleAndDeactivateSystemTray();
  detailed_item_ = NULL;
  UpdateWebNotifications();
}

void SystemTray::DestroyNotificationBubble() {
  if (notification_bubble_) {
    notification_bubble_.reset();
    UpdateWebNotifications();
  }
}

base::string16 SystemTray::GetAccessibleNameForTray() {
  base::string16 time = GetAccessibleTimeString(base::Time::Now());
  base::string16 battery = base::ASCIIToUTF16("");
#if defined(OS_CHROMEOS)
  battery = PowerStatus::Get()->GetAccessibleNameString(false);
#endif
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, time, battery);
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
                                           int arrow_offset,
                                           bool persistent) {
  if (creation_type != BUBBLE_USE_EXISTING) {
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        ash::UMA_STATUS_AREA_MENU_OPENED);
  }
  ShowItems(items_.get(), false, true, creation_type, arrow_offset, persistent);
}

void SystemTray::ShowItems(const std::vector<SystemTrayItem*>& items,
                           bool detailed,
                           bool can_activate,
                           BubbleCreationType creation_type,
                           int arrow_offset,
                           bool persistent) {
  // No system tray bubbles in kiosk mode.
  if (Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus() ==
      ash::user::LOGGED_IN_KIOSK_APP) {
    return;
  }

  // Destroy any existing bubble and create a new one.
  SystemTrayBubble::BubbleType bubble_type = detailed ?
      SystemTrayBubble::BUBBLE_TYPE_DETAILED :
      SystemTrayBubble::BUBBLE_TYPE_DEFAULT;

  // Destroy the notification bubble here so that it doesn't get rebuilt
  // while we add items to the main bubble_ (e.g. in HideNotificationView).
  notification_bubble_.reset();
  if (system_bubble_.get() && creation_type == BUBBLE_USE_EXISTING) {
    system_bubble_->bubble()->UpdateView(items, bubble_type);
    // If ChromeVox is enabled, focus the default item if no item is focused.
    if (Shell::GetInstance()->accessibility_delegate()->
        IsSpokenFeedbackEnabled()) {
      system_bubble_->bubble()->FocusDefaultIfNeeded();
    }
  } else {
    // Remember if the menu is a single property (like e.g. volume) or the
    // full tray menu. Note that in case of the |BUBBLE_USE_EXISTING| case
    // above, |full_system_tray_menu_| does not get changed since the fact that
    // the menu is full (or not) doesn't change even if a "single property"
    // (like network) replaces most of the menu.
    full_system_tray_menu_ = items.size() > 1;
    // The menu width is fixed, and it is a per language setting.
    int menu_width = std::max(kMinimumSystemTrayMenuWidth,
        Shell::GetInstance()->system_tray_delegate()->GetSystemTrayMenuWidth());

    TrayBubbleView::InitParams init_params(TrayBubbleView::ANCHOR_TYPE_TRAY,
                                           GetAnchorAlignment(),
                                           menu_width,
                                           kTrayPopupMaxWidth);
    init_params.can_activate = can_activate;
    init_params.first_item_has_no_margin = true;
    if (detailed) {
      // This is the case where a volume control or brightness control bubble
      // is created.
      init_params.max_height = default_bubble_height_;
      init_params.arrow_color = kBackgroundColor;
    } else {
      init_params.arrow_color = kHeaderBackgroundColor;
    }
    init_params.arrow_offset = arrow_offset;
    if (bubble_type == SystemTrayBubble::BUBBLE_TYPE_DEFAULT)
      init_params.close_on_deactivate = !persistent;
    // For Volume and Brightness we don't want to show an arrow when
    // they are shown in a bubble by themselves.
    init_params.arrow_paint_type = views::BubbleBorder::PAINT_NORMAL;
    if (items.size() == 1 && items[0]->ShouldHideArrow())
      init_params.arrow_paint_type = views::BubbleBorder::PAINT_TRANSPARENT;
    SystemTrayBubble* bubble = new SystemTrayBubble(this, items, bubble_type);
    system_bubble_.reset(new SystemBubbleWrapper(bubble));
    system_bubble_->InitView(this, tray_container(), &init_params, persistent);
  }
  // Save height of default view for creating detailed views directly.
  if (!detailed)
    default_bubble_height_ = system_bubble_->bubble_view()->height();

  if (detailed && items.size() > 0)
    detailed_item_ = items[0];
  else
    detailed_item_ = NULL;

  UpdateNotificationBubble();  // State changed, re-create notifications.
  if (!notification_bubble_)
    UpdateWebNotifications();
  GetShelfLayoutManager()->UpdateAutoHideState();

  // When we show the system menu in our alternate shelf layout, we need to
  // tint the background.
  if (full_system_tray_menu_)
    SetDrawBackgroundAsActive(true);
}

void SystemTray::UpdateNotificationBubble() {
  // Only show the notification bubble if we have notifications.
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
  // Tray items might want to show notifications while we are creating and
  // initializing the |system_bubble_| - but it might not be fully initialized
  // when coming here - this would produce a crashed like crbug.com/247416.
  // As such we check the existence of the widget here.
  if (system_bubble_.get() &&
      system_bubble_->bubble_view() &&
      system_bubble_->bubble_view()->GetWidget()) {
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
  init_params.first_item_has_no_margin = true;
  init_params.arrow_color = kBackgroundColor;
  init_params.arrow_offset = GetTrayXOffset(notification_items_[0]);
  notification_bubble_.reset(new SystemBubbleWrapper(notification_bubble));
  notification_bubble_->InitView(this, anchor, &init_params, false);

  if (notification_bubble->bubble_view()->child_count() == 0) {
    // It is possible that none of the items generated actual notifications.
    DestroyNotificationBubble();
    return;
  }
  if (hide_notifications_)
    notification_bubble->SetVisible(false);
  else
    UpdateWebNotifications();
}

void SystemTray::UpdateWebNotifications() {
  TrayBubbleView* bubble_view = NULL;
  if (notification_bubble_)
    bubble_view = notification_bubble_->bubble_view();
  else if (system_bubble_)
    bubble_view = system_bubble_->bubble_view();

  int height = 0;
  if (bubble_view) {
    gfx::Rect work_area = Shell::GetScreen()->GetDisplayNearestWindow(
        bubble_view->GetWidget()->GetNativeView()).work_area();
    if (GetShelfLayoutManager()->GetAlignment() != SHELF_ALIGNMENT_TOP) {
      height = std::max(
          0, work_area.height() - bubble_view->GetBoundsInScreen().y());
    } else {
      height = std::max(
          0, bubble_view->GetBoundsInScreen().bottom() - work_area.y());
    }
  }
  status_area_widget()->web_notification_tray()->SetSystemTrayHeight(height);
}

base::string16 SystemTray::GetAccessibleTimeString(
    const base::Time& now) const {
  base::HourClockType hour_type =
      ash::Shell::GetInstance()->system_tray_delegate()->GetHourClockType();
  return base::TimeFormatTimeOfDayWithHourClockType(
      now, hour_type, base::kKeepAmPm);
}

void SystemTray::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment == shelf_alignment())
    return;
  TrayBackgroundView::SetShelfAlignment(alignment);
  UpdateAfterShelfAlignmentChange(alignment);
  // Destroy any existing bubble so that it is rebuilt correctly.
  CloseSystemBubbleAndDeactivateSystemTray();
  // Rebuild any notification bubble.
  if (notification_bubble_) {
    notification_bubble_.reset();
    UpdateNotificationBubble();
  }
}

void SystemTray::AnchorUpdated() {
  if (notification_bubble_) {
    notification_bubble_->bubble_view()->UpdateBubble();
    // Ensure that the notification buble is above the shelf/status area.
    notification_bubble_->bubble_view()->GetWidget()->StackAtTop();
    UpdateBubbleViewArrow(notification_bubble_->bubble_view());
  }
  if (system_bubble_) {
    system_bubble_->bubble_view()->UpdateBubble();
    UpdateBubbleViewArrow(system_bubble_->bubble_view());
  }
}

void SystemTray::BubbleResized(const TrayBubbleView* bubble_view) {
  UpdateWebNotifications();
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
  if (!system_bubble_ || system_bubble_->is_persistent())
    return false;
  HideBubbleWithView(system_bubble_->bubble_view());
  return true;
}

void SystemTray::BubbleViewDestroyed() {
  if (system_bubble_) {
    system_bubble_->bubble()->DestroyItemViews();
    system_bubble_->bubble()->BubbleViewDestroyed();
  }
}

void SystemTray::OnMouseEnteredView() {
  if (system_bubble_)
    system_bubble_->bubble()->StopAutoCloseTimer();
}

void SystemTray::OnMouseExitedView() {
  if (system_bubble_)
    system_bubble_->bubble()->RestartAutoCloseTimer();
}

base::string16 SystemTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

gfx::Rect SystemTray::GetAnchorRect(
    views::Widget* anchor_widget,
    TrayBubbleView::AnchorType anchor_type,
    TrayBubbleView::AnchorAlignment anchor_alignment) const {
  return GetBubbleAnchorRect(anchor_widget, anchor_type, anchor_alignment);
}

void SystemTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

views::View* SystemTray::GetTrayItemViewForTest(SystemTrayItem* item) {
  std::map<SystemTrayItem*, views::View*>::iterator it =
      tray_item_map_.find(item);
  return it == tray_item_map_.end() ? NULL : it->second;
}

TrayDate* SystemTray::GetTrayDateForTesting() const { return tray_date_; }

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
    ShowDefaultViewWithOffset(BUBBLE_CREATE_NEW, arrow_offset, false);
  }
  return true;
}

void SystemTray::CloseSystemBubbleAndDeactivateSystemTray() {
  system_bubble_.reset();
  // When closing a system bubble with the alternate shelf layout, we need to
  // turn off the active tinting of the shelf.
  if (full_system_tray_menu_) {
    SetDrawBackgroundAsActive(false);
    full_system_tray_menu_ = false;
  }
}

}  // namespace ash
