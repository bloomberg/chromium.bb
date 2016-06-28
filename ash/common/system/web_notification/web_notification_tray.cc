// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/web_notification/web_notification_tray.h"

#include "ash/common/ash_switches.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_bubble_wrapper.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/common/system/web_notification/ash_popup_alignment_delegate.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/system/tray/system_tray.h"
#include "base/auto_reset.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/message_center/views/message_bubble_base.h"
#include "ui/message_center/views/message_center_bubble.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/fill_layout.h"

#if defined(OS_CHROMEOS)

namespace message_center {

MessageCenterTrayDelegate* CreateMessageCenterTray() {
  // On Windows+Ash the Tray will not be hosted in ash::Shell.
  NOTREACHED();
  return NULL;
}

}  // namespace message_center

#endif  // defined(OS_CHROMEOS)

namespace ash {
namespace {

// Menu commands
const int kToggleQuietMode = 0;
const int kEnableQuietModeDay = 2;
}

namespace {

const SkColor kWebNotificationColorNoUnread =
    SkColorSetARGB(128, 255, 255, 255);
const SkColor kWebNotificationColorWithUnread = SK_ColorWHITE;
const int kNoUnreadIconSize = 18;
}

// Class to initialize and manage the WebNotificationBubble and
// TrayBubbleWrapper instances for a bubble.
class WebNotificationBubbleWrapper {
 public:
  // Takes ownership of |bubble| and creates |bubble_wrapper_|.
  WebNotificationBubbleWrapper(WebNotificationTray* tray,
                               message_center::MessageBubbleBase* bubble) {
    bubble_.reset(bubble);
    views::TrayBubbleView::AnchorAlignment anchor_alignment =
        tray->GetAnchorAlignment();
    views::TrayBubbleView::InitParams init_params =
        bubble->GetInitParams(anchor_alignment);
    views::View* anchor = tray->tray_container();
    if (anchor_alignment == views::TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM) {
      gfx::Point bounds(anchor->width() / 2, 0);
      views::View::ConvertPointToWidget(anchor, &bounds);
      init_params.arrow_offset = bounds.x();
    }
    DCHECK(anchor);
    // TrayBubbleView uses |anchor| and |tray| to determine the parent
    // container. See WebNotificationTray::OnBeforeBubbleWidgetInit().
    views::TrayBubbleView* bubble_view =
        views::TrayBubbleView::Create(anchor, tray, &init_params);
    bubble_wrapper_.reset(new TrayBubbleWrapper(tray, bubble_view));
    bubble_view->SetArrowPaintType(views::BubbleBorder::PAINT_NONE);
    bubble->InitializeContents(bubble_view);
  }

  message_center::MessageBubbleBase* bubble() const { return bubble_.get(); }

  // Convenience accessors.
  views::TrayBubbleView* bubble_view() const { return bubble_->bubble_view(); }

 private:
  std::unique_ptr<message_center::MessageBubbleBase> bubble_;
  std::unique_ptr<TrayBubbleWrapper> bubble_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationBubbleWrapper);
};

class WebNotificationButton : public views::CustomButton {
 public:
  WebNotificationButton(views::ButtonListener* listener)
      : views::CustomButton(listener),
        is_bubble_visible_(false),
        unread_count_(0) {
    SetLayoutManager(new views::FillLayout);

    gfx::ImageSkia image;
    if (MaterialDesignController::IsShelfMaterial()) {
      image = CreateVectorIcon(gfx::VectorIconId::SHELF_NOTIFICATIONS,
                               kShelfIconColor);
    } else {
      image =
          CreateVectorIcon(gfx::VectorIconId::NOTIFICATIONS, kNoUnreadIconSize,
                           kWebNotificationColorNoUnread);
    }

    no_unread_icon_.SetImage(image);
    no_unread_icon_.set_owned_by_client();

    unread_label_.set_owned_by_client();
    SetupLabelForTray(&unread_label_);

    AddChildView(&unread_label_);
  }

  void SetBubbleVisible(bool visible) {
    if (visible == is_bubble_visible_)
      return;

    is_bubble_visible_ = visible;
    UpdateIconVisibility();
  }

  void SetUnreadCount(int unread_count) {
    // base::FormatNumber doesn't convert to arabic numeric characters.
    // TODO(mukai): use ICU to support conversion for such locales.
    unread_count_ = unread_count;
    UpdateIconVisibility();
  }

 protected:
  // Overridden from views::ImageButton:
  gfx::Size GetPreferredSize() const override {
    const int size = GetTrayConstant(TRAY_ITEM_HEIGHT_LEGACY);
    return gfx::Size(size, size);
  }

  int GetHeightForWidth(int width) const override {
    return GetPreferredSize().height();
  }

 private:
  void UpdateIconVisibility() {
    if (unread_count_ == 0) {
      if (!Contains(&no_unread_icon_)) {
        RemoveAllChildViews(false /* delete_children */);
        AddChildView(&no_unread_icon_);
      }
    } else {
      if (!Contains(&unread_label_)) {
        RemoveAllChildViews(false /* delete_children */);
        AddChildView(&unread_label_);
      }

      // TODO(mukai): move NINE_PLUS message to ui_strings, it doesn't need to
      // be in ash_strings.
      unread_label_.SetText(
          (unread_count_ > 9) ? l10n_util::GetStringUTF16(
                                    IDS_ASH_NOTIFICATION_UNREAD_COUNT_NINE_PLUS)
                              : base::FormatNumber(unread_count_));
      unread_label_.SetEnabledColor((unread_count_ > 0)
                                        ? kWebNotificationColorWithUnread
                                        : kWebNotificationColorNoUnread);
    }
    SchedulePaint();
  }

  bool is_bubble_visible_;
  int unread_count_;

  views::ImageView no_unread_icon_;
  views::Label unread_label_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationButton);
};

WebNotificationTray::WebNotificationTray(WmShelf* shelf,
                                         WmWindow* status_area_window,
                                         SystemTray* system_tray)
    : TrayBackgroundView(shelf),
      status_area_window_(status_area_window),
      system_tray_(system_tray),
      button_(nullptr),
      show_message_center_on_unlock_(false),
      should_update_tray_content_(false),
      should_block_shelf_auto_hide_(false) {
  DCHECK(shelf);
  DCHECK(status_area_window_);
  DCHECK(system_tray_);
  button_ = new WebNotificationButton(this);
  button_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                       ui::EF_RIGHT_MOUSE_BUTTON);
  tray_container()->AddChildView(button_);
  button_->SetFocusBehavior(FocusBehavior::NEVER);
  SetContentsBackground();
  tray_container()->SetBorder(views::Border::NullBorder());
  message_center_tray_.reset(new message_center::MessageCenterTray(
      this, message_center::MessageCenter::Get()));
  popup_alignment_delegate_.reset(new AshPopupAlignmentDelegate(shelf));
  popup_collection_.reset(new message_center::MessagePopupCollection(
      message_center(), message_center_tray_.get(),
      popup_alignment_delegate_.get()));
  const display::Display& display =
      status_area_window_->GetDisplayNearestWindow();
  popup_alignment_delegate_->StartObserving(display::Screen::GetScreen(),
                                            display);
  OnMessageCenterTrayChanged();
}

WebNotificationTray::~WebNotificationTray() {
  // Release any child views that might have back pointers before ~View().
  message_center_bubble_.reset();
  popup_alignment_delegate_.reset();
  popup_collection_.reset();
}

// Public methods.

bool WebNotificationTray::ShowMessageCenterInternal(bool show_settings) {
  if (!ShouldShowMessageCenter())
    return false;

  should_block_shelf_auto_hide_ = true;
  message_center::MessageCenterBubble* message_center_bubble =
      new message_center::MessageCenterBubble(message_center(),
                                              message_center_tray_.get(), true);

  int max_height;
  if (IsHorizontalAlignment(shelf()->GetAlignment())) {
    max_height = shelf()->GetIdealBounds().y();
  } else {
    // Assume the status area and bubble bottoms are aligned when vertical.
    gfx::Rect bounds_in_root =
        status_area_window_->GetRootWindow()->ConvertRectFromScreen(
            status_area_window_->GetBoundsInScreen());
    max_height = bounds_in_root.bottom();
  }
  message_center_bubble->SetMaxHeight(
      std::max(0, max_height - GetTrayConstant(TRAY_SPACING)));
  if (show_settings)
    message_center_bubble->SetSettingsVisible();
  message_center_bubble_.reset(
      new WebNotificationBubbleWrapper(this, message_center_bubble));

  system_tray_->SetHideNotifications(true);
  shelf()->UpdateAutoHideState();
  button_->SetBubbleVisible(true);
  SetDrawBackgroundAsActive(true);
  return true;
}

bool WebNotificationTray::ShowMessageCenter() {
  return ShowMessageCenterInternal(false /* show_settings */);
}

void WebNotificationTray::HideMessageCenter() {
  if (!message_center_bubble())
    return;
  SetDrawBackgroundAsActive(false);
  message_center_bubble_.reset();
  should_block_shelf_auto_hide_ = false;
  show_message_center_on_unlock_ = false;
  system_tray_->SetHideNotifications(false);
  shelf()->UpdateAutoHideState();
  button_->SetBubbleVisible(false);
}

void WebNotificationTray::SetTrayBubbleHeight(int height) {
  popup_alignment_delegate_->SetTrayBubbleHeight(height);
}

int WebNotificationTray::tray_bubble_height_for_test() const {
  return popup_alignment_delegate_->tray_bubble_height_for_test();
}

bool WebNotificationTray::ShowPopups() {
  if (message_center_bubble())
    return false;

  popup_collection_->DoUpdateIfPossible();
  return true;
}

void WebNotificationTray::HidePopups() {
  DCHECK(popup_collection_.get());
  popup_collection_->MarkAllPopupsShown();
}

// Private methods.

bool WebNotificationTray::ShouldShowMessageCenter() {
  return !system_tray_->HasNotificationBubble();
}

bool WebNotificationTray::ShouldBlockShelfAutoHide() const {
  return should_block_shelf_auto_hide_;
}

bool WebNotificationTray::IsMessageCenterBubbleVisible() const {
  return (message_center_bubble() &&
          message_center_bubble()->bubble()->IsVisible());
}

bool WebNotificationTray::IsMouseInNotificationBubble() const {
  return false;
}

void WebNotificationTray::ShowMessageCenterBubble() {
  if (!IsMessageCenterBubbleVisible())
    message_center_tray_->ShowMessageCenterBubble();
}

void WebNotificationTray::UpdateAfterLoginStatusChange(
    LoginStatus login_status) {
  message_center()->SetLockedState(login_status == LoginStatus::LOCKED);
  OnMessageCenterTrayChanged();
}

void WebNotificationTray::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment == shelf_alignment())
    return;
  TrayBackgroundView::SetShelfAlignment(alignment);
  tray_container()->SetBorder(views::Border::NullBorder());
  // Destroy any existing bubble so that it will be rebuilt correctly.
  message_center_tray_->HideMessageCenterBubble();
  message_center_tray_->HidePopupBubble();
}

void WebNotificationTray::AnchorUpdated() {
  if (message_center_bubble()) {
    message_center_bubble()->bubble_view()->UpdateBubble();
    UpdateBubbleViewArrow(message_center_bubble()->bubble_view());
  }
}

base::string16 WebNotificationTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_ACCESSIBLE_NAME);
}

void WebNotificationTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {
  if (message_center_bubble() &&
      bubble_view == message_center_bubble()->bubble_view()) {
    message_center_tray_->HideMessageCenterBubble();
  } else if (popup_collection_.get()) {
    message_center_tray_->HidePopupBubble();
  }
}

bool WebNotificationTray::PerformAction(const ui::Event& event) {
  if (message_center_bubble())
    message_center_tray_->HideMessageCenterBubble();
  else
    message_center_tray_->ShowMessageCenterBubble();
  return true;
}

void WebNotificationTray::BubbleViewDestroyed() {
  if (message_center_bubble())
    message_center_bubble()->bubble()->BubbleViewDestroyed();
}

void WebNotificationTray::OnMouseEnteredView() {}

void WebNotificationTray::OnMouseExitedView() {}

base::string16 WebNotificationTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

gfx::Rect WebNotificationTray::GetAnchorRect(
    views::Widget* anchor_widget,
    views::TrayBubbleView::AnchorType anchor_type,
    views::TrayBubbleView::AnchorAlignment anchor_alignment) const {
  return GetBubbleAnchorRect(anchor_widget, anchor_type, anchor_alignment);
}

void WebNotificationTray::OnBeforeBubbleWidgetInit(
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

void WebNotificationTray::HideBubble(const views::TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

bool WebNotificationTray::ShowNotifierSettings() {
  if (message_center_bubble()) {
    static_cast<message_center::MessageCenterBubble*>(
        message_center_bubble()->bubble())
        ->SetSettingsVisible();
    return true;
  }
  return ShowMessageCenterInternal(true /* show_settings */);
}

bool WebNotificationTray::IsContextMenuEnabled() const {
  return IsLoggedIn();
}

message_center::MessageCenterTray* WebNotificationTray::GetMessageCenterTray() {
  return message_center_tray_.get();
}

bool WebNotificationTray::IsCommandIdChecked(int command_id) const {
  if (command_id != kToggleQuietMode)
    return false;
  return message_center()->IsQuietMode();
}

bool WebNotificationTray::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool WebNotificationTray::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void WebNotificationTray::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == kToggleQuietMode) {
    bool in_quiet_mode = message_center()->IsQuietMode();
    message_center()->SetQuietMode(!in_quiet_mode);
    return;
  }
  base::TimeDelta expires_in = command_id == kEnableQuietModeDay
                                   ? base::TimeDelta::FromDays(1)
                                   : base::TimeDelta::FromHours(1);
  message_center()->EnterQuietModeWithExpire(expires_in);
}

void WebNotificationTray::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  DCHECK_EQ(button_, sender);
  PerformAction(event);
}

void WebNotificationTray::OnMessageCenterTrayChanged() {
  // Do not update the tray contents directly. Multiple change events can happen
  // consecutively, and calling Update in the middle of those events will show
  // intermediate unread counts for a moment.
  should_update_tray_content_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&WebNotificationTray::UpdateTrayContent, AsWeakPtr()));
}

void WebNotificationTray::UpdateTrayContent() {
  if (!should_update_tray_content_)
    return;
  should_update_tray_content_ = false;

  message_center::MessageCenter* message_center =
      message_center_tray_->message_center();
  button_->SetUnreadCount(message_center->UnreadNotificationCount());
  if (IsMessageCenterBubbleVisible())
    button_->SetState(views::CustomButton::STATE_PRESSED);
  else
    button_->SetState(views::CustomButton::STATE_NORMAL);

  SetVisible(IsLoggedIn());
  Layout();
  SchedulePaint();
  if (IsLoggedIn())
    system_tray_->SetNextFocusableView(this);
}

void WebNotificationTray::ClickedOutsideBubble() {
  // Only hide the message center
  if (!message_center_bubble())
    return;

  message_center_tray_->HideMessageCenterBubble();
}

message_center::MessageCenter* WebNotificationTray::message_center() const {
  return message_center_tray_->message_center();
}

bool WebNotificationTray::IsLoggedIn() const {
  WmShell* shell = WmShell::Get();
  // TODO(jamescook): Should this also check LoginState::LOCKED?
  return shell->system_tray_delegate()->GetUserLoginStatus() !=
             LoginStatus::NOT_LOGGED_IN &&
         !shell->GetSessionStateDelegate()->IsInSecondaryLoginScreen();
}

// Methods for testing

bool WebNotificationTray::IsPopupVisible() const {
  return message_center_tray_->popups_visible();
}

message_center::MessageCenterBubble*
WebNotificationTray::GetMessageCenterBubbleForTest() {
  if (!message_center_bubble())
    return NULL;
  return static_cast<message_center::MessageCenterBubble*>(
      message_center_bubble()->bubble());
}

}  // namespace ash
