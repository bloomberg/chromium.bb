// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/system_tray.h"

#include <algorithm>
#include <map>
#include <vector>

#include "ash/common/key_event_watcher.h"
#include "ash/common/login_status.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/chromeos/audio/tray_audio.h"
#include "ash/common/system/chromeos/bluetooth/tray_bluetooth.h"
#include "ash/common/system/chromeos/brightness/tray_brightness.h"
#include "ash/common/system/chromeos/cast/tray_cast.h"
#include "ash/common/system/chromeos/enterprise/tray_enterprise.h"
#include "ash/common/system/chromeos/media_security/multi_profile_media_tray_item.h"
#include "ash/common/system/chromeos/network/tray_network.h"
#include "ash/common/system/chromeos/network/tray_vpn.h"
#include "ash/common/system/chromeos/power/power_status.h"
#include "ash/common/system/chromeos/power/tray_power.h"
#include "ash/common/system/chromeos/screen_security/screen_capture_tray_item.h"
#include "ash/common/system/chromeos/screen_security/screen_share_tray_item.h"
#include "ash/common/system/chromeos/session/tray_session_length_limit.h"
#include "ash/common/system/chromeos/settings/tray_settings.h"
#include "ash/common/system/chromeos/supervised/tray_supervised_user.h"
#include "ash/common/system/chromeos/tray_caps_lock.h"
#include "ash/common/system/chromeos/tray_tracing.h"
#include "ash/common/system/date/tray_date.h"
#include "ash/common/system/date/tray_system_info.h"
#include "ash/common/system/ime/tray_ime_chromeos.h"
#include "ash/common/system/tiles/tray_tiles.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/system/tray/tray_bubble_wrapper.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray_accessibility.h"
#include "ash/common/system/update/tray_update.h"
#include "ash/common/system/user/tray_user.h"
#include "ash/common/system/web_notification/web_notification_tray.h"
#include "ash/common/wm/container_finder.h"
#include "ash/common/wm_activation_observer.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/timer.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_style.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using views::TrayBubbleView;

namespace ash {

namespace {

// A tray item that just reserves space in the tray.
class PaddingTrayItem : public SystemTrayItem {
 public:
  PaddingTrayItem() : SystemTrayItem(nullptr, UMA_NOT_RECORDED) {}
  ~PaddingTrayItem() override {}

  // SystemTrayItem:
  views::View* CreateTrayView(LoginStatus status) override {
    return new PaddingView();
  }

 private:
  class PaddingView : public views::View {
   public:
    PaddingView() {}
    ~PaddingView() override {}

   private:
    gfx::Size GetPreferredSize() const override {
      // The other tray items already have some padding baked in so we have to
      // subtract that off.
      const int side = kTrayEdgePadding - kTrayImageItemPadding;
      return gfx::Size(side, side);
    }

    DISALLOW_COPY_AND_ASSIGN(PaddingView);
  };

  DISALLOW_COPY_AND_ASSIGN(PaddingTrayItem);
};

}  // namespace

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
                const gfx::Insets& anchor_insets,
                TrayBubbleView::InitParams* init_params,
                bool is_persistent) {
    DCHECK(anchor);
    LoginStatus login_status =
        WmShell::Get()->system_tray_delegate()->GetUserLoginStatus();
    bubble_->InitView(anchor, login_status, init_params);
    bubble_->bubble_view()->set_anchor_view_insets(anchor_insets);
    bubble_wrapper_.reset(new TrayBubbleWrapper(tray, bubble_->bubble_view()));
    is_persistent_ = is_persistent;

    // If ChromeVox is enabled, focus the default item if no item is focused and
    // there isn't a delayed close.
    if (WmShell::Get()->accessibility_delegate()->IsSpokenFeedbackEnabled() &&
        !is_persistent) {
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
  std::unique_ptr<SystemTrayBubble> bubble_;
  std::unique_ptr<TrayBubbleWrapper> bubble_wrapper_;
  bool is_persistent_;

  DISALLOW_COPY_AND_ASSIGN(SystemBubbleWrapper);
};

// An activation observer to close the bubble if the window other
// than system bubble nor popup notification is activated.
class SystemTray::ActivationObserver : public WmActivationObserver {
 public:
  explicit ActivationObserver(SystemTray* tray) : tray_(tray) {
    DCHECK(tray_);
    WmShell::Get()->AddActivationObserver(this);
  }

  ~ActivationObserver() override {
    WmShell::Get()->RemoveActivationObserver(this);
  }

  // WmActivationObserver:
  void OnWindowActivated(WmWindow* gained_active,
                         WmWindow* lost_active) override {
    if (!tray_->HasSystemBubble() || !gained_active)
      return;

    int container_id =
        wm::GetContainerForWindow(gained_active)->GetShellWindowId();

    // Don't close the bubble if a popup notification is activated.
    if (container_id == kShellWindowId_StatusContainer)
      return;

    if (tray_->GetSystemBubble()->bubble_view()->GetWidget() !=
        gained_active->GetInternalWidget()) {
      tray_->CloseSystemBubble();
    }
  }
  void OnAttemptToReactivateWindow(WmWindow* request_active,
                                   WmWindow* actual_active) override {}

 private:
  SystemTray* tray_;

  DISALLOW_COPY_AND_ASSIGN(ActivationObserver);
};

// SystemTray

SystemTray::SystemTray(WmShelf* wm_shelf)
    : TrayBackgroundView(wm_shelf),
      web_notification_tray_(nullptr),
      detailed_item_(nullptr),
      default_bubble_height_(0),
      full_system_tray_menu_(false),
      tray_accessibility_(nullptr),
      tray_audio_(nullptr),
      tray_cast_(nullptr),
      tray_date_(nullptr),
      tray_network_(nullptr),
      tray_tiles_(nullptr),
      tray_system_info_(nullptr),
      tray_update_(nullptr),
      screen_capture_tray_item_(nullptr),
      screen_share_tray_item_(nullptr) {
  if (MaterialDesignController::IsShelfMaterial()) {
    SetInkDropMode(InkDropMode::ON);
    SetContentsBackground(false);

    // Since user avatar is on the right hand side of System tray of a
    // horizontal shelf and that is sufficient to indicate separation, no
    // separator is required.
    set_separator_visibility(false);
  } else {
    SetContentsBackground(true);
  }
}

SystemTray::~SystemTray() {
  // Destroy any child views that might have back pointers before ~View().
  activation_observer_.reset();
  key_event_watcher_.reset();
  system_bubble_.reset();
  for (const auto& item : items_)
    item->DestroyTrayView();
}

void SystemTray::InitializeTrayItems(
    SystemTrayDelegate* delegate,
    WebNotificationTray* web_notification_tray) {
  DCHECK(web_notification_tray);
  web_notification_tray_ = web_notification_tray;
  TrayBackgroundView::Initialize();
  CreateItems(delegate);
}

void SystemTray::Shutdown() {
  DCHECK(web_notification_tray_);
  web_notification_tray_ = nullptr;
}

void SystemTray::CreateItems(SystemTrayDelegate* delegate) {
  const bool use_md = MaterialDesignController::IsSystemTrayMenuMaterial();

  // Create user items for each possible user.
  int maximum_user_profiles = WmShell::Get()
                                  ->GetSessionStateDelegate()
                                  ->GetMaximumNumberOfLoggedInUsers();
  for (int i = 0; i < maximum_user_profiles; i++)
    AddTrayItem(base::MakeUnique<TrayUser>(this, i));

  // Crucially, this trailing padding has to be inside the user item(s).
  // Otherwise it could be a main axis margin on the tray's box layout.
  AddTrayItem(base::MakeUnique<PaddingTrayItem>());

  tray_accessibility_ = new TrayAccessibility(this);
  if (!use_md)
    tray_date_ = new TrayDate(this);
  tray_update_ = new TrayUpdate(this);

  AddTrayItem(base::MakeUnique<TraySessionLengthLimit>(this));
  AddTrayItem(base::MakeUnique<TrayEnterprise>(this));
  AddTrayItem(base::MakeUnique<TraySupervisedUser>(this));
  AddTrayItem(base::MakeUnique<TrayIME>(this));
  AddTrayItem(base::WrapUnique(tray_accessibility_));
  AddTrayItem(base::MakeUnique<TrayTracing>(this));
  AddTrayItem(
      base::MakeUnique<TrayPower>(this, message_center::MessageCenter::Get()));
  tray_network_ = new TrayNetwork(this);
  AddTrayItem(base::WrapUnique(tray_network_));
  AddTrayItem(base::MakeUnique<TrayVPN>(this));
  AddTrayItem(base::MakeUnique<TrayBluetooth>(this));
  tray_cast_ = new TrayCast(this);
  AddTrayItem(base::WrapUnique(tray_cast_));
  screen_capture_tray_item_ = new ScreenCaptureTrayItem(this);
  AddTrayItem(base::WrapUnique(screen_capture_tray_item_));
  screen_share_tray_item_ = new ScreenShareTrayItem(this);
  AddTrayItem(base::WrapUnique(screen_share_tray_item_));
  AddTrayItem(base::MakeUnique<MultiProfileMediaTrayItem>(this));
  tray_audio_ = new TrayAudio(this);
  AddTrayItem(base::WrapUnique(tray_audio_));
  AddTrayItem(base::MakeUnique<TrayBrightness>(this));
  AddTrayItem(base::MakeUnique<TrayCapsLock>(this));
  // TODO(jamescook): Remove this when mus has support for display management
  // and we have a DisplayManager equivalent. See http://crbug.com/548429
  std::unique_ptr<SystemTrayItem> tray_rotation_lock =
      delegate->CreateRotationLockTrayItem(this);
  if (tray_rotation_lock)
    AddTrayItem(std::move(tray_rotation_lock));
  if (!use_md)
    AddTrayItem(base::MakeUnique<TraySettings>(this));
  AddTrayItem(base::WrapUnique(tray_update_));
  if (use_md) {
    tray_tiles_ = new TrayTiles(this);
    AddTrayItem(base::WrapUnique(tray_tiles_));
    tray_system_info_ = new TraySystemInfo(this);
    AddTrayItem(base::WrapUnique(tray_system_info_));
    // Leading padding.
    AddTrayItem(base::MakeUnique<PaddingTrayItem>());
  } else {
    AddTrayItem(base::WrapUnique(tray_date_));
  }
}

void SystemTray::AddTrayItem(std::unique_ptr<SystemTrayItem> item) {
  SystemTrayItem* item_ptr = item.get();
  items_.push_back(std::move(item));

  SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
  views::View* tray_item =
      item_ptr->CreateTrayView(delegate->GetUserLoginStatus());
  item_ptr->UpdateAfterShelfAlignmentChange(shelf_alignment());

  if (tray_item) {
    tray_container()->AddChildViewAt(tray_item, 0);
    PreferredSizeChanged();
    tray_item_map_[item_ptr] = tray_item;
  }
}

std::vector<SystemTrayItem*> SystemTray::GetTrayItems() const {
  std::vector<SystemTrayItem*> result;
  for (const auto& item : items_)
    result.push_back(item.get());
  return result;
}

void SystemTray::ShowDefaultView(BubbleCreationType creation_type) {
  if (creation_type != BUBBLE_USE_EXISTING)
    WmShell::Get()->RecordUserMetricsAction(UMA_STATUS_AREA_MENU_OPENED);
  ShowItems(GetTrayItems(), false, true, creation_type, false);
}

void SystemTray::ShowPersistentDefaultView() {
  ShowItems(GetTrayItems(), false, false, BUBBLE_CREATE_NEW, true);
}

void SystemTray::ShowDetailedView(SystemTrayItem* item,
                                  int close_delay,
                                  bool activate,
                                  BubbleCreationType creation_type) {
  std::vector<SystemTrayItem*> items;
  // The detailed view with timeout means a UI to show the current system state,
  // like the audio level or brightness. Such UI should behave as persistent and
  // keep its own logic for the appearance.
  bool persistent =
      (!activate && close_delay > 0 && creation_type == BUBBLE_CREATE_NEW);
  items.push_back(item);
  ShowItems(items, true, activate, creation_type, persistent);
  if (system_bubble_)
    system_bubble_->bubble()->StartAutoCloseTimer(close_delay);
}

void SystemTray::SetDetailedViewCloseDelay(int close_delay) {
  if (HasSystemBubbleType(SystemTrayBubble::BUBBLE_TYPE_DETAILED))
    system_bubble_->bubble()->StartAutoCloseTimer(close_delay);
}

void SystemTray::HideDetailedView(SystemTrayItem* item, bool animate) {
  if (item != detailed_item_)
    return;

  if (!animate) {
    // In unittest, GetSystemBubble might return nullptr.
    if (GetSystemBubble()) {
      GetSystemBubble()
          ->bubble_view()
          ->GetWidget()
          ->SetVisibilityAnimationTransition(
              views::Widget::VisibilityTransition::ANIMATE_NONE);
    }
  }

  DestroySystemBubble();
}

void SystemTray::UpdateAfterLoginStatusChange(LoginStatus login_status) {
  DestroySystemBubble();

  for (const auto& item : items_)
    item->UpdateAfterLoginStatusChange(login_status);

  // Items default to SHELF_ALIGNMENT_BOTTOM. Update them if the initial
  // position of the shelf differs.
  if (!IsHorizontalAlignment(shelf_alignment()))
    UpdateAfterShelfAlignmentChange(shelf_alignment());

  SetVisible(true);
  PreferredSizeChanged();
}

void SystemTray::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  for (const auto& item : items_)
    item->UpdateAfterShelfAlignmentChange(alignment);
}

bool SystemTray::ShouldShowShelf() const {
  return system_bubble_.get() && system_bubble_->bubble()->ShouldShowShelf();
}

bool SystemTray::HasSystemBubble() const {
  return system_bubble_.get() != NULL;
}

SystemTrayBubble* SystemTray::GetSystemBubble() {
  if (!system_bubble_)
    return NULL;
  return system_bubble_->bubble();
}

bool SystemTray::IsSystemBubbleVisible() const {
  return HasSystemBubble() && system_bubble_->bubble()->IsVisible();
}

bool SystemTray::CloseSystemBubble() const {
  if (!system_bubble_)
    return false;
  CHECK(!activating_);
  system_bubble_->bubble()->Close();
  return true;
}

views::View* SystemTray::GetHelpButtonView() const {
  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    return tray_tiles_->GetHelpButtonView();
  return tray_date_->GetHelpButtonView();
}

TrayAudio* SystemTray::GetTrayAudio() const {
  return tray_audio_;
}

// Private methods.

bool SystemTray::HasSystemBubbleType(SystemTrayBubble::BubbleType type) {
  return system_bubble_.get() && system_bubble_->bubble_type() == type;
}

void SystemTray::DestroySystemBubble() {
  CloseSystemBubbleAndDeactivateSystemTray();
  detailed_item_ = NULL;
  UpdateWebNotifications();
}

base::string16 SystemTray::GetAccessibleNameForTray() {
  base::string16 time = GetAccessibleTimeString(base::Time::Now());
  base::string16 battery = PowerStatus::Get()->GetAccessibleNameString(false);
  return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION,
                                    time, battery);
}

void SystemTray::ShowItems(const std::vector<SystemTrayItem*>& items,
                           bool detailed,
                           bool can_activate,
                           BubbleCreationType creation_type,
                           bool persistent) {
  // No system tray bubbles in kiosk mode.
  SystemTrayDelegate* system_tray_delegate =
      WmShell::Get()->system_tray_delegate();
  if (system_tray_delegate->GetUserLoginStatus() == LoginStatus::KIOSK_APP ||
      system_tray_delegate->GetUserLoginStatus() ==
          LoginStatus::ARC_KIOSK_APP) {
    return;
  }

  // Destroy any existing bubble and create a new one.
  SystemTrayBubble::BubbleType bubble_type =
      detailed ? SystemTrayBubble::BUBBLE_TYPE_DETAILED
               : SystemTrayBubble::BUBBLE_TYPE_DEFAULT;

  if (system_bubble_.get() && creation_type == BUBBLE_USE_EXISTING) {
    system_bubble_->bubble()->UpdateView(items, bubble_type);
    // If ChromeVox is enabled, focus the default item if no item is focused.
    if (WmShell::Get()->accessibility_delegate()->IsSpokenFeedbackEnabled())
      system_bubble_->bubble()->FocusDefaultIfNeeded();
  } else {
    // Cleanup the existing bubble before showing a new one. Otherwise, it's
    // possible to confuse the new system bubble with the old one during
    // destruction, leading to subtle errors/crashes such as crbug.com/545166.
    DestroySystemBubble();

    // Remember if the menu is a single property (like e.g. volume) or the
    // full tray menu. Note that in case of the |BUBBLE_USE_EXISTING| case
    // above, |full_system_tray_menu_| does not get changed since the fact that
    // the menu is full (or not) doesn't change even if a "single property"
    // (like network) replaces most of the menu.
    full_system_tray_menu_ = items.size() > 1;

    // The menu width is fixed for all languages in material design.
    int menu_width = kTrayMenuMinimumWidthMd;
    if (!MaterialDesignController::IsSystemTrayMenuMaterial()) {
      // The menu width is fixed, and it is a per language setting.
      menu_width = std::max(
          kTrayMenuMinimumWidth,
          WmShell::Get()->system_tray_delegate()->GetSystemTrayMenuWidth());
    }

    TrayBubbleView::InitParams init_params(GetAnchorAlignment(), menu_width,
                                           kTrayPopupMaxWidth);
    // TODO(oshima): Change TrayBubbleView itself.
    init_params.can_activate = false;
    if (detailed) {
      // This is the case where a volume control or brightness control bubble
      // is created.
      init_params.max_height = default_bubble_height_;
      init_params.bg_color = kBackgroundColor;
    } else {
      init_params.bg_color = kHeaderBackgroundColor;
    }
    if (bubble_type == SystemTrayBubble::BUBBLE_TYPE_DEFAULT)
      init_params.close_on_deactivate = !persistent;
    SystemTrayBubble* bubble = new SystemTrayBubble(this, items, bubble_type);

    system_bubble_.reset(new SystemBubbleWrapper(bubble));
    system_bubble_->InitView(this, GetBubbleAnchor(), GetBubbleAnchorInsets(),
                             &init_params, persistent);

    activation_observer_.reset(persistent ? nullptr
                                          : new ActivationObserver(this));

    // Record metrics for the system menu when the default view is invoked.
    if (!detailed)
      RecordSystemMenuMetrics();
  }
  // Save height of default view for creating detailed views directly.
  if (!detailed)
    default_bubble_height_ = system_bubble_->bubble_view()->height();

  key_event_watcher_.reset();
  if (can_activate)
    CreateKeyEventWatcher();

  if (detailed && items.size() > 0)
    detailed_item_ = items[0];
  else
    detailed_item_ = NULL;

  UpdateWebNotifications();
  shelf()->UpdateAutoHideState();

  // When we show the system menu in our alternate shelf layout, we need to
  // tint the background.
  if (full_system_tray_menu_)
    SetIsActive(true);
}

void SystemTray::UpdateWebNotifications() {
  TrayBubbleView* bubble_view = NULL;
  if (system_bubble_)
    bubble_view = system_bubble_->bubble_view();

  int height = 0;
  if (bubble_view) {
    gfx::Rect work_area =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(bubble_view->GetWidget()->GetNativeView())
            .work_area();
    height =
        std::max(0, work_area.bottom() - bubble_view->GetBoundsInScreen().y());
  }
  if (web_notification_tray_)
    web_notification_tray_->SetTrayBubbleHeight(height);
}

base::string16 SystemTray::GetAccessibleTimeString(
    const base::Time& now) const {
  base::HourClockType hour_type =
      WmShell::Get()->system_tray_controller()->hour_clock_type();
  return base::TimeFormatTimeOfDayWithHourClockType(now, hour_type,
                                                    base::kKeepAmPm);
}

void SystemTray::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment == shelf_alignment())
    return;
  TrayBackgroundView::SetShelfAlignment(alignment);
  UpdateAfterShelfAlignmentChange(alignment);
  // Destroy any existing bubble so that it is rebuilt correctly.
  CloseSystemBubbleAndDeactivateSystemTray();
  // Rebuild any notification bubble.
  UpdateWebNotifications();
}

void SystemTray::AnchorUpdated() {
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
    shelf()->UpdateAutoHideState();
  }
}

void SystemTray::ClickedOutsideBubble() {
  if (!system_bubble_ || system_bubble_->is_persistent())
    return;
  HideBubbleWithView(system_bubble_->bubble_view());
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

void SystemTray::OnBeforeBubbleWidgetInit(
    views::Widget* anchor_widget,
    views::Widget* bubble_widget,
    views::Widget::InitParams* params) const {
  // Place the bubble in the same root window as |anchor_widget|.
  WmLookup::Get()
      ->GetWindowForWidget(anchor_widget)
      ->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          bubble_widget, kShellWindowId_SettingBubbleContainer, params);
}

void SystemTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

views::View* SystemTray::GetTrayItemViewForTest(SystemTrayItem* item) {
  std::map<SystemTrayItem*, views::View*>::iterator it =
      tray_item_map_.find(item);
  return it == tray_item_map_.end() ? NULL : it->second;
}

TrayCast* SystemTray::GetTrayCastForTesting() const {
  return tray_cast_;
}

TrayDate* SystemTray::GetTrayDateForTesting() const {
  return tray_date_;
}

TrayNetwork* SystemTray::GetTrayNetworkForTesting() const {
  return tray_network_;
}

TraySystemInfo* SystemTray::GetTraySystemInfoForTesting() const {
  return tray_system_info_;
}

TrayTiles* SystemTray::GetTrayTilesForTesting() const {
  return tray_tiles_;
}

void SystemTray::CloseBubble(const ui::KeyEvent& key_event) {
  CloseSystemBubble();
}

void SystemTray::ActivateAndStartNavigation(const ui::KeyEvent& key_event) {
  if (!system_bubble_)
    return;
  activating_ = true;
  ActivateBubble();
  activating_ = false;
  // TODO(oshima): This is to troubleshoot the issue crbug.com/651242. Remove
  // once the root cause is fixed.
  CHECK(system_bubble_) << " the bubble was deleted while activaing it";

  views::Widget* widget = GetSystemBubble()->bubble_view()->GetWidget();
  widget->GetFocusManager()->OnKeyEvent(key_event);
}

void SystemTray::CreateKeyEventWatcher() {
  key_event_watcher_ = WmShell::Get()->CreateKeyEventWatcher();
  // mustash does not yet support KeyEventWatcher. http://crbug.com/649600.
  if (!key_event_watcher_)
    return;
  key_event_watcher_->AddKeyEventCallback(
      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE),
      base::Bind(&SystemTray::CloseBubble, base::Unretained(this)));
  key_event_watcher_->AddKeyEventCallback(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_NONE),
      base::Bind(&SystemTray::ActivateAndStartNavigation,
                 base::Unretained(this)));
  key_event_watcher_->AddKeyEventCallback(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_SHIFT_DOWN),
      base::Bind(&SystemTray::ActivateAndStartNavigation,
                 base::Unretained(this)));
}

void SystemTray::ActivateBubble() {
  TrayBubbleView* bubble_view = GetSystemBubble()->bubble_view();
  bubble_view->set_can_activate(true);
  bubble_view->GetWidget()->Activate();
}

bool SystemTray::PerformAction(const ui::Event& event) {
  // If we're already showing the default view, hide it; otherwise, show it
  // (and hide any popup that's currently shown).
  if (HasSystemBubbleType(SystemTrayBubble::BUBBLE_TYPE_DEFAULT)) {
    system_bubble_->bubble()->Close();
  } else {
    ShowDefaultView(BUBBLE_CREATE_NEW);
    if (event.IsKeyEvent() || (event.flags() & ui::EF_TOUCH_ACCESSIBILITY))
      ActivateBubble();
  }
  return true;
}

void SystemTray::CloseSystemBubbleAndDeactivateSystemTray() {
  CHECK(!activating_);
  activation_observer_.reset();
  key_event_watcher_.reset();
  system_bubble_.reset();
  // When closing a system bubble with the alternate shelf layout, we need to
  // turn off the active tinting of the shelf.
  if (full_system_tray_menu_) {
    SetIsActive(false);
    full_system_tray_menu_ = false;
  }
}

void SystemTray::RecordSystemMenuMetrics() {
  DCHECK(system_bubble_);

  system_bubble_->bubble()->RecordVisibleRowMetrics();

  TrayBubbleView* bubble_view = system_bubble_->bubble_view();
  int num_rows = 0;
  for (int i = 0; i < bubble_view->child_count(); i++) {
    // Certain menu rows are attached by default but can set themselves as
    // invisible (IME is one such example). Count only user-visible rows.
    if (bubble_view->child_at(i)->visible())
      num_rows++;
  }
  UMA_HISTOGRAM_COUNTS_100("Ash.SystemMenu.Rows", num_rows);

  int work_area_height =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(bubble_view->GetWidget()->GetNativeView())
          .work_area()
          .height();
  if (work_area_height > 0) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Ash.SystemMenu.PercentageOfWorkAreaHeightCoveredByMenu",
        100 * bubble_view->height() / work_area_height, 1, 300, 100);
  }
}

}  // namespace ash
