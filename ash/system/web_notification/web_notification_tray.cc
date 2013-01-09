// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_tray.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "ash/wm/shelf_layout_manager.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/screen.h"
#include "ui/message_center/message_center_bubble.h"
#include "ui/message_center/message_popup_bubble.h"
#include "ui/message_center/quiet_mode_bubble.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/widget/widget_observer.h"

namespace {

// Tray constants
const int kTrayContainerVerticalPaddingBottomAlignment = 3;
const int kTrayContainerHorizontalPaddingBottomAlignment = 1;
const int kTrayContainerVerticalPaddingVerticalAlignment = 1;
const int kTrayContainerHorizontalPaddingVerticalAlignment = 0;
const int kPaddingFromLeftEdgeOfSystemTrayBottomAlignment = 8;
const int kPaddingFromTopEdgeOfSystemTrayVerticalAlignment = 10;

}  // namespace

namespace ash {

namespace internal {

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
    views::TrayBubbleView* bubble_view = views::TrayBubbleView::Create(
        tray->GetBubbleWindowContainer(), anchor, tray, &init_params);
    bubble_wrapper_.reset(new TrayBubbleWrapper(tray, bubble_view));
    bubble->InitializeContents(bubble_view);
  }

  message_center::MessageBubbleBase* bubble() const { return bubble_.get(); }

  // Convenience accessors.
  views::TrayBubbleView* bubble_view() const { return bubble_->bubble_view(); }

 private:
  scoped_ptr<message_center::MessageBubbleBase> bubble_;
  scoped_ptr<internal::TrayBubbleWrapper> bubble_wrapper_;
};

}  // namespace internal

WebNotificationTray::WebNotificationTray(
    internal::StatusAreaWidget* status_area_widget)
    : internal::TrayBackgroundView(status_area_widget),
      button_(NULL),
      show_message_center_on_unlock_(false) {
  message_center_ = message_center::MessageCenter::GetInstance();
  message_center_->AddObserver(this);
  button_ = new views::ImageButton(this);
  button_->set_triggerable_event_flags(
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON);
  tray_container()->AddChildView(button_);
  SetVisible(false);
  UpdateTray();
}

WebNotificationTray::~WebNotificationTray() {
  // Ensure the message center doesn't notify a destroyed object.
  message_center_->RemoveObserver(this);
  // Release any child views that might have back pointers before ~View().
  message_center_bubble_.reset();
  popup_bubble_.reset();
  if (quiet_mode_bubble() && quiet_mode_bubble_->GetBubbleWidget())
    quiet_mode_bubble_->GetBubbleWidget()->RemoveObserver(this);
  quiet_mode_bubble_.reset();
}

void WebNotificationTray::ShowMessageCenterBubble() {
  if (status_area_widget()->login_status() == user::LOGGED_IN_LOCKED)
    return;
  if (quiet_mode_bubble())
    quiet_mode_bubble_.reset();
  if (message_center_bubble()) {
    UpdateTray();
    return;
  }
  // Indicate that the message center is visible. Clears the unread count.
  message_center_->SetMessageCenterVisible(true);
  UpdateTray();
  HidePopupBubble();
  message_center::MessageCenterBubble* bubble =
      new message_center::MessageCenterBubble(message_center_);
  message_center_bubble_.reset(
      new internal::WebNotificationBubbleWrapper(this, bubble));

  status_area_widget()->SetHideSystemNotifications(true);
  GetShelfLayoutManager()->UpdateAutoHideState();
}

void WebNotificationTray::HideMessageCenterBubble() {
  if (!message_center_bubble())
    return;
  message_center_bubble_.reset();
  show_message_center_on_unlock_ = false;
  message_center_->SetMessageCenterVisible(false);
  UpdateTray();
  status_area_widget()->SetHideSystemNotifications(false);
  GetShelfLayoutManager()->UpdateAutoHideState();
}

void WebNotificationTray::SetHidePopupBubble(bool hide) {
  if (hide)
    HidePopupBubble();
  else
    ShowPopupBubble();
}

void WebNotificationTray::ShowPopupBubble() {
  if (status_area_widget()->login_status() == user::LOGGED_IN_LOCKED)
    return;
  if (message_center_bubble())
    return;
  if (!status_area_widget()->ShouldShowWebNotifications())
    return;
  UpdateTray();
  if (popup_bubble()) {
    popup_bubble()->bubble()->ScheduleUpdate();
  } else if (message_center_->HasPopupNotifications()) {
    popup_bubble_.reset(
        new internal::WebNotificationBubbleWrapper(
            this, new message_center::MessagePopupBubble(
                message_center_)));
  }
}

void WebNotificationTray::HidePopupBubble() {
  popup_bubble_.reset();
}

bool WebNotificationTray::ShouldShowQuietModeBubble(const ui::Event& event) {
  // TODO(mukai): Add keyboard event handler.
  if (!event.IsMouseEvent())
    return false;

  const ui::MouseEvent* mouse_event =
      static_cast<const ui::MouseEvent*>(&event);

  return mouse_event->IsRightMouseButton();
}

void WebNotificationTray::ShowQuietModeBubble() {
  aura::Window* parent = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(),
      internal::kShellWindowId_SettingBubbleContainer);
  quiet_mode_bubble_.reset(new message_center::QuietModeBubble(
      button_, parent, message_center_->notification_list()));
  quiet_mode_bubble_->GetBubbleWidget()->StackAtTop();
  quiet_mode_bubble_->GetBubbleWidget()->AddObserver(this);
}

void WebNotificationTray::UpdateAfterLoginStatusChange(
    user::LoginStatus login_status) {
  if (login_status == user::LOGGED_IN_LOCKED) {
    if (message_center_bubble()) {
      message_center_bubble_.reset();
      show_message_center_on_unlock_ = true;
    }
    HidePopupBubble();
  } else {
    if (show_message_center_on_unlock_)
      ShowMessageCenterBubble();
    show_message_center_on_unlock_ = false;
  }
  // The status icon should be always visible except for lock screen / login
  // screen, to allow quiet mode and settings.
  SetVisible((login_status != user::LOGGED_IN_NONE) &&
             (login_status != user::LOGGED_IN_LOCKED));
  UpdateTray();
}

bool WebNotificationTray::ShouldBlockLauncherAutoHide() const {
  return IsMessageCenterBubbleVisible() || quiet_mode_bubble() != NULL;
}

bool WebNotificationTray::IsMessageCenterBubbleVisible() const {
  return (message_center_bubble() &&
          message_center_bubble_->bubble()->IsVisible());
}

bool WebNotificationTray::IsMouseInNotificationBubble() const {
  if (!popup_bubble())
    return false;
  return popup_bubble_->bubble_view()->GetBoundsInScreen().Contains(
      Shell::GetScreen()->GetCursorScreenPoint());
}

void WebNotificationTray::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment == shelf_alignment())
    return;
  internal::TrayBackgroundView::SetShelfAlignment(alignment);
  // Destroy any existing bubble so that it will be rebuilt correctly.
  HideMessageCenterBubble();
  HidePopupBubble();
}

void WebNotificationTray::AnchorUpdated() {
  if (popup_bubble_.get()) {
    popup_bubble_->bubble_view()->UpdateBubble();
    // Ensure that the notification buble is above the launcher/status area.
    popup_bubble_->bubble_view()->GetWidget()->StackAtTop();
    UpdateBubbleViewArrow(popup_bubble_->bubble_view());
  }
  if (message_center_bubble_.get()) {
    message_center_bubble_->bubble_view()->UpdateBubble();
    UpdateBubbleViewArrow(message_center_bubble_->bubble_view());
  }
  // Quiet mode settings bubble has to be on top.
  if (quiet_mode_bubble() && quiet_mode_bubble_->GetBubbleWidget())
    quiet_mode_bubble_->GetBubbleWidget()->StackAtTop();
}

string16 WebNotificationTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_WEB_NOTIFICATION_TRAY_ACCESSIBLE_NAME);
}

void WebNotificationTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {
  if (message_center_bubble() &&
      bubble_view == message_center_bubble()->bubble_view()) {
    HideMessageCenterBubble();
  } else if (popup_bubble() && bubble_view == popup_bubble()->bubble_view()) {
    HidePopupBubble();
  }
}

bool WebNotificationTray::PerformAction(const ui::Event& event) {
  if (!quiet_mode_bubble() && ShouldShowQuietModeBubble(event)) {
    ShowQuietModeBubble();
    return true;
  }
  quiet_mode_bubble_.reset();
  ToggleMessageCenterBubble();
  return true;
}

void WebNotificationTray::BubbleViewDestroyed() {
  if (message_center_bubble())
    message_center_bubble()->bubble()->BubbleViewDestroyed();
  if (popup_bubble())
    popup_bubble()->bubble()->BubbleViewDestroyed();
}

void WebNotificationTray::OnMouseEnteredView() {
  if (popup_bubble())
    popup_bubble()->bubble()->OnMouseEnteredView();
}

void WebNotificationTray::OnMouseExitedView() {
  if (popup_bubble())
    popup_bubble()->bubble()->OnMouseExitedView();
}

string16 WebNotificationTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

gfx::Rect WebNotificationTray::GetAnchorRect(views::Widget* anchor_widget,
                                             AnchorType anchor_type,
                                             AnchorAlignment anchor_alignment) {
  return GetBubbleAnchorRect(anchor_widget, anchor_type, anchor_alignment);
}

void WebNotificationTray::HideBubble(const views::TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void WebNotificationTray::OnMessageCenterChanged(bool new_notification) {
  if (message_center_bubble()) {
    if (message_center_->NotificationCount() == 0)
      HideMessageCenterBubble();
    else
      message_center_bubble()->bubble()->ScheduleUpdate();
  }
  if (popup_bubble()) {
    if (message_center_->NotificationCount() == 0)
      HidePopupBubble();
    else
      popup_bubble()->bubble()->ScheduleUpdate();
  }
  UpdateTray();
  if (new_notification)
    ShowPopupBubble();
}

void WebNotificationTray::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  DCHECK_EQ(button_, sender);
  PerformAction(event);
}

void WebNotificationTray::OnWidgetClosing(views::Widget* widget) {
  if (quiet_mode_bubble() && quiet_mode_bubble_->GetBubbleWidget() == widget) {
    widget->RemoveObserver(this);
  }
  quiet_mode_bubble_.reset();
}

// Private methods

void WebNotificationTray::ToggleMessageCenterBubble() {
  if (message_center_bubble())
    HideMessageCenterBubble();
  else
    ShowMessageCenterBubble();
  UpdateTray();
}

void WebNotificationTray::UpdateTray() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (message_center_->UnreadNotificationCount() > 0) {
    button_->SetImage(views::CustomButton::STATE_NORMAL, rb.GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_NOTIFY_BUTTON_ACTIVE_NORMAL));
    button_->SetImage(views::CustomButton::STATE_HOVERED, rb.GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_NOTIFY_BUTTON_ACTIVE_HOVER));
    button_->SetImage(views::CustomButton::STATE_PRESSED, rb.GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_NOTIFY_BUTTON_ACTIVE_PRESSED));
  } else {
    button_->SetImage(views::CustomButton::STATE_NORMAL, rb.GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_NOTIFY_BUTTON_INACTIVE_NORMAL));
    button_->SetImage(views::CustomButton::STATE_HOVERED, rb.GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_NOTIFY_BUTTON_INACTIVE_HOVER));
    button_->SetImage(views::CustomButton::STATE_PRESSED, rb.GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_NOTIFY_BUTTON_INACTIVE_PRESSED));
  }
  if (message_center_bubble())
    button_->SetState(views::CustomButton::STATE_PRESSED);
  else
    button_->SetState(views::CustomButton::STATE_NORMAL);
  Layout();
  SchedulePaint();
}

bool WebNotificationTray::ClickedOutsideBubble() {
  // Only hide the message center and quiet mode bubble.
  if (!message_center_bubble() && !quiet_mode_bubble())
    return false;
  quiet_mode_bubble_.reset();
  HideMessageCenterBubble();
  return true;
}

// Methods for testing

message_center::MessageCenterBubble*
WebNotificationTray::GetMessageCenterBubbleForTest() {
  if (!message_center_bubble_.get())
    return NULL;
  return static_cast<message_center::MessageCenterBubble*>(
      message_center_bubble_->bubble());
}

message_center::MessagePopupBubble*
WebNotificationTray::GetPopupBubbleForTest() {
  if (!popup_bubble_.get())
    return NULL;
  return static_cast<message_center::MessagePopupBubble*>(
      popup_bubble_->bubble());
}

}  // namespace ash
