// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray.h"

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include "ash/login_status.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/audio/tray_audio.h"
#include "ash/system/bluetooth/tray_bluetooth.h"
#include "ash/system/brightness/tray_brightness.h"
#include "ash/system/cast/tray_cast.h"
#include "ash/system/date/tray_system_info.h"
#include "ash/system/display_scale/tray_scale.h"
#include "ash/system/enterprise/tray_enterprise.h"
#include "ash/system/ime/tray_ime_chromeos.h"
#include "ash/system/keyboard_brightness/tray_keyboard_brightness.h"
#include "ash/system/media_security/multi_profile_media_tray_item.h"
#include "ash/system/network/tray_network.h"
#include "ash/system/network/tray_vpn.h"
#include "ash/system/night_light/tray_night_light.h"
#include "ash/system/power/power_status.h"
#include "ash/system/power/tray_power.h"
#include "ash/system/rotation/tray_rotation_lock.h"
#include "ash/system/screen_security/screen_capture_tray_item.h"
#include "ash/system/screen_security/screen_share_tray_item.h"
#include "ash/system/screen_security/screen_tray_item.h"
#include "ash/system/session/tray_session_length_limit.h"
#include "ash/system/supervised/tray_supervised_user.h"
#include "ash/system/tiles/tray_tiles.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/tray_caps_lock.h"
#include "ash/system/tray_tracing.h"
#include "ash/system/update/tray_update.h"
#include "ash/system/user/tray_user.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/widget_finder.h"
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
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

using views::TrayBubbleView;

namespace ash {

namespace {

// Dialog that confirms the user wants to stop screen share/cast. Calls a
// callback with the result.
class CancelCastingDialog : public views::DialogDelegateView {
 public:
  explicit CancelCastingDialog(base::OnceCallback<void(bool)> callback)
      : callback_(std::move(callback)) {
    AddChildView(new views::MessageBoxView(views::MessageBoxView::InitParams(
        l10n_util::GetStringUTF16(IDS_DESKTOP_CASTING_ACTIVE_MESSAGE))));
    SetLayoutManager(new views::FillLayout());
  }
  ~CancelCastingDialog() override = default;

  base::string16 GetWindowTitle() const override {
    return l10n_util::GetStringUTF16(IDS_DESKTOP_CASTING_ACTIVE_TITLE);
  }

  int GetDialogButtons() const override {
    return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
  }

  bool Cancel() override {
    std::move(callback_).Run(false);
    return true;
  }

  bool Accept() override {
    // Stop screen sharing and capturing.
    SystemTray* system_tray = Shell::Get()->GetPrimarySystemTray();
    if (system_tray->GetScreenShareItem()->is_started())
      system_tray->GetScreenShareItem()->Stop();
    if (system_tray->GetScreenCaptureItem()->is_started())
      system_tray->GetScreenCaptureItem()->Stop();

    std::move(callback_).Run(true);
    return true;
  }

 private:
  base::OnceCallback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(CancelCastingDialog);
};

// A tray item that just reserves space in the tray.
class PaddingTrayItem : public SystemTrayItem {
 public:
  PaddingTrayItem() : SystemTrayItem(nullptr, UMA_NOT_RECORDED) {}
  ~PaddingTrayItem() override {}

  // SystemTrayItem:
  views::View* CreateTrayView(LoginStatus status) override {
    auto* padding = new views::View();
    // The other tray items already have some padding baked in so we have to
    // subtract that off.
    constexpr int side = kTrayEdgePadding - kTrayImageItemPadding;
    padding->SetPreferredSize(gfx::Size(side, side));
    return padding;
  }

 private:
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

    is_persistent_ = is_persistent;

    const LoginStatus login_status =
        Shell::Get()->session_controller()->login_status();
    bubble_->InitView(anchor, login_status, init_params);
    bubble_->bubble_view()->set_anchor_view_insets(anchor_insets);
    bubble_wrapper_.reset(new TrayBubbleWrapper(tray, bubble_->bubble_view()));
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
class SystemTray::ActivationObserver : public ::wm::ActivationChangeObserver {
 public:
  explicit ActivationObserver(SystemTray* tray) : tray_(tray) {
    DCHECK(tray_);
    Shell::Get()->activation_client()->AddObserver(this);
  }

  ~ActivationObserver() override {
    Shell::Get()->activation_client()->RemoveObserver(this);
  }

  // WmActivationObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    if (!tray_->HasSystemBubble() || !gained_active)
      return;

    int container_id = wm::GetContainerForWindow(gained_active)->id();

    // Don't close the bubble if a popup notification is activated.
    if (container_id == kShellWindowId_StatusContainer ||
        container_id == kShellWindowId_SettingBubbleContainer) {
      return;
    }

    views::Widget* bubble_widget =
        tray_->GetSystemBubble()->bubble_view()->GetWidget();
    // Don't close the bubble if a transient child is gaining or losing
    // activation.
    if (bubble_widget == GetInternalWidgetForWindow(gained_active) ||
        ::wm::HasTransientAncestor(gained_active,
                                   bubble_widget->GetNativeWindow()) ||
        (lost_active && ::wm::HasTransientAncestor(
                            lost_active, bubble_widget->GetNativeWindow()))) {
      return;
    }

    tray_->CloseBubble();
  }

 private:
  SystemTray* tray_;

  DISALLOW_COPY_AND_ASSIGN(ActivationObserver);
};

// SystemTray

SystemTray::SystemTray(Shelf* shelf) : TrayBackgroundView(shelf) {
  SetInkDropMode(InkDropMode::ON);

  // Since user avatar is on the right hand side of System tray of a
  // horizontal shelf and that is sufficient to indicate separation, no
  // separator is required.
  set_separator_visibility(false);
}

SystemTray::~SystemTray() {
  // Destroy any child views that might have back pointers before ~View().
  activation_observer_.reset();
  system_bubble_.reset();
  for (const auto& item : items_)
    item->OnTrayViewDestroyed();
}

void SystemTray::InitializeTrayItems(
    WebNotificationTray* web_notification_tray) {
  DCHECK(web_notification_tray);
  web_notification_tray_ = web_notification_tray;
  TrayBackgroundView::Initialize();
  CreateItems();
}

void SystemTray::Shutdown() {
  DCHECK(web_notification_tray_);
  web_notification_tray_ = nullptr;
}

void SystemTray::CreateItems() {
  AddTrayItem(std::make_unique<TrayUser>(this));

  // Crucially, this trailing padding has to be inside the user item(s).
  // Otherwise it could be a main axis margin on the tray's box layout.
  AddTrayItem(std::make_unique<PaddingTrayItem>());

  tray_session_length_limit_ = new TraySessionLengthLimit(this);
  AddTrayItem(base::WrapUnique(tray_session_length_limit_));
  tray_enterprise_ = new TrayEnterprise(this);
  AddTrayItem(base::WrapUnique(tray_enterprise_));
  tray_supervised_user_ = new TraySupervisedUser(this);
  AddTrayItem(base::WrapUnique(tray_supervised_user_));
  AddTrayItem(std::make_unique<TrayIME>(this));
  tray_accessibility_ = new TrayAccessibility(this);
  AddTrayItem(base::WrapUnique(tray_accessibility_));
  tray_tracing_ = new TrayTracing(this);
  AddTrayItem(base::WrapUnique(tray_tracing_));
  AddTrayItem(
      std::make_unique<TrayPower>(this, message_center::MessageCenter::Get()));
  tray_network_ = new TrayNetwork(this);
  AddTrayItem(base::WrapUnique(tray_network_));
  AddTrayItem(std::make_unique<TrayVPN>(this));
  AddTrayItem(std::make_unique<TrayBluetooth>(this));
  tray_cast_ = new TrayCast(this);
  AddTrayItem(base::WrapUnique(tray_cast_));
  screen_capture_tray_item_ = new ScreenCaptureTrayItem(this);
  AddTrayItem(base::WrapUnique(screen_capture_tray_item_));
  screen_share_tray_item_ = new ScreenShareTrayItem(this);
  AddTrayItem(base::WrapUnique(screen_share_tray_item_));
  AddTrayItem(std::make_unique<MultiProfileMediaTrayItem>(this));
  tray_audio_ = new TrayAudio(this);
  AddTrayItem(base::WrapUnique(tray_audio_));
  tray_scale_ = new TrayScale(this);
  AddTrayItem(base::WrapUnique(tray_scale_));
  AddTrayItem(std::make_unique<TrayBrightness>(this));
  AddTrayItem(std::make_unique<TrayKeyboardBrightness>(this));
  tray_caps_lock_ = new TrayCapsLock(this);
  AddTrayItem(base::WrapUnique(tray_caps_lock_));
  if (NightLightController::IsFeatureEnabled()) {
    tray_night_light_ = new TrayNightLight(this);
    AddTrayItem(base::WrapUnique(tray_night_light_));
  }
  // TODO(jamescook): Remove this when mash has support for display management
  // and we have a DisplayManager equivalent. See http://crbug.com/548429
  if (Shell::GetAshConfig() != Config::MASH)
    AddTrayItem(std::make_unique<TrayRotationLock>(this));
  tray_update_ = new TrayUpdate(this);
  AddTrayItem(base::WrapUnique(tray_update_));
  tray_tiles_ = new TrayTiles(this);
  AddTrayItem(base::WrapUnique(tray_tiles_));
  tray_system_info_ = new TraySystemInfo(this);
  AddTrayItem(base::WrapUnique(tray_system_info_));
  // Leading padding.
  AddTrayItem(std::make_unique<PaddingTrayItem>());
}

void SystemTray::AddTrayItem(std::unique_ptr<SystemTrayItem> item) {
  SystemTrayItem* item_ptr = item.get();
  items_.push_back(std::move(item));

  views::View* tray_item = item_ptr->CreateTrayView(
      Shell::Get()->session_controller()->login_status());
  item_ptr->UpdateAfterShelfAlignmentChange();

  if (tray_item) {
    tray_container()->AddChildViewAt(tray_item, 0);
    PreferredSizeChanged();
  }
}

std::vector<SystemTrayItem*> SystemTray::GetTrayItems() const {
  std::vector<SystemTrayItem*> result;
  for (const auto& item : items_)
    result.push_back(item.get());
  return result;
}

void SystemTray::ShowDefaultView(BubbleCreationType creation_type,
                                 bool show_by_click) {
  if (creation_type != BUBBLE_USE_EXISTING)
    Shell::Get()->metrics()->RecordUserMetricsAction(
        UMA_STATUS_AREA_MENU_OPENED);
  ShowItems(GetTrayItems(), false, creation_type, false, show_by_click);
}

void SystemTray::ShowPersistentDefaultView() {
  ShowItems(GetTrayItems(), false, BUBBLE_CREATE_NEW, true, false);
}

void SystemTray::ShowDetailedView(SystemTrayItem* item,
                                  int close_delay,
                                  BubbleCreationType creation_type) {
  std::vector<SystemTrayItem*> items;
  // The detailed view with timeout means a UI to show the current system state,
  // like the audio level or brightness. Such UI should behave as persistent and
  // keep its own logic for the appearance.
  bool persistent = (close_delay > 0 && creation_type == BUBBLE_CREATE_NEW);
  items.push_back(item);
  ShowItems(items, true, creation_type, persistent, false);
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

  SetVisible(true);
  PreferredSizeChanged();
}

void SystemTray::UpdateItemsAfterShelfAlignmentChange() {
  for (const auto& item : items_)
    item->UpdateAfterShelfAlignmentChange();
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

views::View* SystemTray::GetHelpButtonView() const {
  return tray_tiles_->GetHelpButtonView();
}

TrayAudio* SystemTray::GetTrayAudio() const {
  return tray_audio_;
}

void SystemTray::CanSwitchAwayFromActiveUser(
    base::OnceCallback<void(bool)> callback) {
  // If neither screen sharing nor capturing is going on we can immediately
  // switch users.
  if (!GetScreenShareItem()->is_started() &&
      !GetScreenCaptureItem()->is_started()) {
    std::move(callback).Run(true);
    return;
  }

  views::DialogDelegate::CreateDialogWidget(
      new CancelCastingDialog(std::move(callback)),
      Shell::GetPrimaryRootWindow(), nullptr)
      ->Show();
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
                           BubbleCreationType creation_type,
                           bool persistent,
                           bool show_by_click) {
  // No system tray bubbles in kiosk app mode.
  if (Shell::Get()->session_controller()->IsRunningInAppMode())
    return;

  // Destroy any existing bubble and create a new one.
  SystemTrayBubble::BubbleType bubble_type =
      detailed ? SystemTrayBubble::BUBBLE_TYPE_DETAILED
               : SystemTrayBubble::BUBBLE_TYPE_DEFAULT;

  if (system_bubble_.get() && creation_type == BUBBLE_USE_EXISTING) {
    system_bubble_->bubble()->UpdateView(items, bubble_type);
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

    TrayBubbleView::InitParams init_params;
    init_params.anchor_alignment = GetAnchorAlignment();
    init_params.min_width = kTrayMenuWidth;
    init_params.max_width = kTrayMenuWidth;
    // The bubble is not initially activatable, but will become activatable if
    // the user presses Tab. For behavioral consistency with the non-activatable
    // scenario, don't close on deactivation after Tab either.
    init_params.close_on_deactivate = false;
    init_params.show_by_click = show_by_click;
    if (detailed) {
      // This is the case where a volume control or brightness control bubble
      // is created.
      init_params.max_height = default_bubble_height_;
    } else {
      init_params.bg_color = kHeaderBackgroundColor;
    }
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
    gfx::Rect work_area = display::Screen::GetScreen()
                              ->GetDisplayNearestWindow(
                                  bubble_view->GetWidget()->GetNativeWindow())
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
      Shell::Get()->system_tray_controller()->hour_clock_type();
  return base::TimeFormatTimeOfDayWithHourClockType(now, hour_type,
                                                    base::kKeepAmPm);
}

void SystemTray::UpdateAfterShelfAlignmentChange() {
  TrayBackgroundView::UpdateAfterShelfAlignmentChange();
  UpdateItemsAfterShelfAlignmentChange();
  // Destroy any existing bubble so that it is rebuilt correctly.
  CloseSystemBubbleAndDeactivateSystemTray();
  // Rebuild any notification bubble.
  UpdateWebNotifications();
}

void SystemTray::AnchorUpdated() {
  if (system_bubble_) {
    UpdateClippingWindowBounds();
    system_bubble_->bubble_view()->UpdateBubble();
    // Should check |system_bubble_| again here. Since UpdateBubble above
    // set the bounds of the bubble which will stop the current animation.
    // If the system tray bubble is during animation to close,
    // CloseBubbleObserver in TrayBackgroundView will close the bubble if
    // animation finished.
    if (system_bubble_)
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

bool SystemTray::PerformAction(const ui::Event& event) {
  // If we're already showing a full system tray menu, either default or
  // detailed menu, hide it; otherwise, show it (and hide any popup that's
  // currently shown).
  if (HasSystemBubble() && full_system_tray_menu_) {
    system_bubble_->bubble()->Close();
  } else {
    ShowBubble(event.IsMouseEvent() || event.IsGestureEvent());
    if (event.IsKeyEvent() || (event.flags() & ui::EF_TOUCH_ACCESSIBILITY))
      ActivateBubble();
  }
  return true;
}

void SystemTray::CloseBubble() {
  if (!system_bubble_)
    return;
  system_bubble_->bubble()->Close();
}

void SystemTray::ShowBubble(bool show_by_click) {
  ShowDefaultView(BUBBLE_CREATE_NEW, show_by_click);
}

views::TrayBubbleView* SystemTray::GetBubbleView() {
  // Only return the bubble view when it's showing the main system tray bubble,
  // not the volume or brightness bubbles etc., to avoid client confusion.
  return system_bubble_ && full_system_tray_menu_
             ? system_bubble_->bubble_view()
             : nullptr;
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

bool SystemTray::ShouldEnableExtraKeyboardAccessibility() {
  // Do not enable extra keyboard accessibility for persistent system bubble.
  // e.g. volume slider. Persistent system bubble is a bubble which is not
  // closed even if user clicks outside of the bubble.
  return system_bubble_ && !system_bubble_->is_persistent() &&
         Shell::Get()->accessibility_delegate()->IsSpokenFeedbackEnabled();
}

void SystemTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void SystemTray::ActivateAndStartNavigation(const ui::KeyEvent& key_event) {
  if (!system_bubble_)
    return;
  ActivateBubble();

  views::Widget* widget = GetSystemBubble()->bubble_view()->GetWidget();
  widget->GetFocusManager()->OnKeyEvent(key_event);
}

void SystemTray::ActivateBubble() {
  TrayBubbleView* bubble_view = GetSystemBubble()->bubble_view();
  // If system tray bubble is in the process of closing, do not try to activate
  // bubble.
  if (bubble_view->GetWidget()->IsClosed())
    return;
  bubble_view->set_can_activate(true);
  bubble_view->GetWidget()->Activate();
}

void SystemTray::CloseSystemBubbleAndDeactivateSystemTray() {
  activation_observer_.reset();
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
          ->GetDisplayNearestWindow(bubble_view->GetWidget()->GetNativeWindow())
          .work_area()
          .height();
  if (work_area_height > 0) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Ash.SystemMenu.PercentageOfWorkAreaHeightCoveredByMenu",
        100 * bubble_view->height() / work_area_height, 1, 300, 100);
  }
}

}  // namespace ash
