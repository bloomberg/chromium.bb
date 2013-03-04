// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_tray.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/shelf_layout_manager.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/screen.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/message_center/message_center_util.h"
#include "ui/message_center/views/message_bubble_base.h"
#include "ui/message_center/views/message_center_bubble.h"
#include "ui/message_center/views/message_popup_bubble.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/message_center/views/quiet_mode_bubble.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget_observer.h"

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
    : TrayBackgroundView(status_area_widget),
      button_(NULL),
      show_message_center_on_unlock_(false) {
  button_ = new views::ImageButton(this);
  button_->set_triggerable_event_flags(
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON);
  tray_container()->AddChildView(button_);
  SetVisible(false);
  message_center_tray_.reset(new message_center::MessageCenterTray(
      this,
      Shell::GetInstance()->message_center()));
  OnMessageCenterTrayChanged();
}

WebNotificationTray::~WebNotificationTray() {
  // Release any child views that might have back pointers before ~View().
  message_center_bubble_.reset();
  popup_bubble_.reset();
  popup_collection_.reset();
  if (quiet_mode_bubble() && quiet_mode_bubble()->GetBubbleWidget())
    quiet_mode_bubble()->GetBubbleWidget()->RemoveObserver(this);
  quiet_mode_bubble_.reset();
}

// Public methods.

bool WebNotificationTray::ShowMessageCenter() {
  if (!ShouldShowMessageCenter())
    return false;

  message_center::MessageCenterBubble* message_center_bubble =
      new message_center::MessageCenterBubble(message_center());

  // TODO(mukai): move this to WebNotificationBubbleWrapper if it's safe
  // to set the height of the popup.
  int max_height = 0;
  aura::Window* status_area_window = status_area_widget()->GetNativeWindow();
  switch (GetShelfLayoutManager()->GetAlignment()) {
    case SHELF_ALIGNMENT_BOTTOM: {
      gfx::Rect shelf_bounds = GetShelfLayoutManager()->GetIdealBounds();
      max_height = shelf_bounds.y();
      break;
    }
    case SHELF_ALIGNMENT_TOP: {
      aura::RootWindow* root = status_area_window->GetRootWindow();
      max_height =
          root->bounds().height() - status_area_window->bounds().height();
      break;
    }
    case SHELF_ALIGNMENT_LEFT:
    case SHELF_ALIGNMENT_RIGHT: {
      // Assume that the bottom line of the status area widget and the bubble
      // are aligned.
      max_height = status_area_window->GetBoundsInRootWindow().bottom();
      break;
    }
    default:
      NOTREACHED();
  }
  max_height = std::max(0, max_height - kTraySpacing);
  message_center_bubble->SetMaxHeight(max_height);
  message_center_bubble_.reset(
      new internal::WebNotificationBubbleWrapper(this, message_center_bubble));

  status_area_widget()->SetHideSystemNotifications(true);
  GetShelfLayoutManager()->UpdateAutoHideState();
  return true;
}

void WebNotificationTray::UpdateMessageCenter() {
  if (message_center_bubble())
    message_center_bubble()->bubble()->ScheduleUpdate();
}

void WebNotificationTray::HideMessageCenter() {
  if (!message_center_bubble())
    return;
  message_center_bubble_.reset();
  show_message_center_on_unlock_ = false;
  status_area_widget()->SetHideSystemNotifications(false);
  GetShelfLayoutManager()->UpdateAutoHideState();
}

void WebNotificationTray::SetHidePopupBubble(bool hide) {
  if (hide)
    message_center_tray_->HidePopupBubble();
  else
    message_center_tray_->ShowPopupBubble();
}

bool WebNotificationTray::ShowPopups() {
  if (status_area_widget()->login_status() == user::LOGGED_IN_LOCKED ||
      message_center_bubble() ||
      !status_area_widget()->ShouldShowWebNotifications()) {
    return false;
  }
  if (message_center::IsRichNotificationEnabled()) {
    // No bubble wrappers here, since |popup_collection_| is not a bubble but a
    // collection of widgets.
    popup_collection_.reset(new message_center::MessagePopupCollection(
        GetWidget()->GetNativeView(), message_center()));
  } else {
    message_center::MessagePopupBubble* popup_bubble =
        new message_center::MessagePopupBubble(message_center());
    popup_bubble_.reset(new internal::WebNotificationBubbleWrapper(
        this, popup_bubble));
  }
  return true;
}

void WebNotificationTray::UpdatePopups() {
  if (popup_bubble())
    popup_bubble()->bubble()->ScheduleUpdate();
  if (popup_collection_.get())
    popup_collection_->UpdatePopups();
};

void WebNotificationTray::HidePopups() {
  popup_bubble_.reset();
  popup_collection_.reset();
}

// Private methods.

bool WebNotificationTray::ShouldShowMessageCenter() {
  return status_area_widget()->login_status() != user::LOGGED_IN_LOCKED &&
      status_area_widget()->ShouldShowWebNotifications();
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
      button_,
      parent,
      message_center_tray_->message_center()->notification_list()));
  quiet_mode_bubble()->GetBubbleWidget()->StackAtTop();
  quiet_mode_bubble()->GetBubbleWidget()->AddObserver(this);
}

void WebNotificationTray::UpdateAfterLoginStatusChange(
    user::LoginStatus login_status) {
  if (message_center::IsRichNotificationEnabled()) {
    // The status icon should be always visible except for lock screen / login
    // screen, to allow quiet mode and settings. This is valid only when rich
    // notification is enabled, since old UI doesn't have settings.
    SetVisible((login_status != user::LOGGED_IN_NONE) &&
               (login_status != user::LOGGED_IN_LOCKED));
  }

  if (login_status == user::LOGGED_IN_LOCKED) {
    show_message_center_on_unlock_ =
        message_center_tray_->HideMessageCenterBubble();
    message_center_tray_->HidePopupBubble();
  } else {
    // Only try once to show the message center bubble on login status change,
    // so always set |show_message_center_on_unlock_| to false.
    if (show_message_center_on_unlock_)
      message_center_tray_->ShowMessageCenterBubble();
    show_message_center_on_unlock_ = false;
  }
}

bool WebNotificationTray::ShouldBlockLauncherAutoHide() const {
  return IsMessageCenterBubbleVisible() || quiet_mode_bubble() != NULL;
}

bool WebNotificationTray::IsMessageCenterBubbleVisible() const {
  return (message_center_bubble() &&
          message_center_bubble()->bubble()->IsVisible());
}

bool WebNotificationTray::IsMouseInNotificationBubble() const {
  if (!popup_bubble())
    return false;
  return popup_bubble()->bubble_view()->GetBoundsInScreen().Contains(
      Shell::GetScreen()->GetCursorScreenPoint());
}

void WebNotificationTray::ShowMessageCenterBubble() {
  if (!IsMessageCenterBubbleVisible())
    message_center_tray_->ShowMessageCenterBubble();
}

void WebNotificationTray::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment == shelf_alignment())
    return;
  internal::TrayBackgroundView::SetShelfAlignment(alignment);
  // Destroy any existing bubble so that it will be rebuilt correctly.
  message_center_tray_->HideMessageCenterBubble();
  message_center_tray_->HidePopupBubble();
}

void WebNotificationTray::AnchorUpdated() {
  if (popup_bubble()) {
    popup_bubble()->bubble_view()->UpdateBubble();
    // Ensure that the notification buble is above the launcher/status area.
    popup_bubble()->bubble_view()->GetWidget()->StackAtTop();
    UpdateBubbleViewArrow(popup_bubble()->bubble_view());
  }
  if (popup_collection_.get())
    popup_collection_->UpdatePopups();
  if (message_center_bubble()) {
    message_center_bubble()->bubble_view()->UpdateBubble();
    UpdateBubbleViewArrow(message_center_bubble()->bubble_view());
  }
  // Quiet mode settings bubble has to be on top.
  if (quiet_mode_bubble() && quiet_mode_bubble()->GetBubbleWidget())
    quiet_mode_bubble()->GetBubbleWidget()->StackAtTop();
}

string16 WebNotificationTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_WEB_NOTIFICATION_TRAY_ACCESSIBLE_NAME);
}

void WebNotificationTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {
  if (message_center_bubble() &&
      bubble_view == message_center_bubble()->bubble_view()) {
    message_center_tray_->HideMessageCenterBubble();
  } else if ((popup_bubble() && bubble_view == popup_bubble()->bubble_view()) ||
             popup_collection_.get()) {
    message_center_tray_->HidePopupBubble();
  }
}

bool WebNotificationTray::PerformAction(const ui::Event& event) {
  if (!quiet_mode_bubble() && ShouldShowQuietModeBubble(event)) {
    ShowQuietModeBubble();
    return true;
  }
  quiet_mode_bubble_.reset();
  if (message_center_bubble())
    message_center_tray_->HideMessageCenterBubble();
  else
    message_center_tray_->ShowMessageCenterBubble();
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

gfx::Rect WebNotificationTray::GetAnchorRect(
    views::Widget* anchor_widget,
    views::TrayBubbleView::AnchorType anchor_type,
    views::TrayBubbleView::AnchorAlignment anchor_alignment) {
  return GetBubbleAnchorRect(anchor_widget, anchor_type, anchor_alignment);
}

void WebNotificationTray::HideBubble(const views::TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void WebNotificationTray::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  DCHECK_EQ(button_, sender);
  PerformAction(event);
}

void WebNotificationTray::OnWidgetDestroying(views::Widget* widget) {
  if (quiet_mode_bubble() && quiet_mode_bubble()->GetBubbleWidget() == widget) {
    widget->RemoveObserver(this);
  }
  quiet_mode_bubble_.reset();
}

void WebNotificationTray::OnMessageCenterTrayChanged() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  message_center::MessageCenter* message_center =
      message_center_tray_->message_center();
  if (message_center->UnreadNotificationCount() > 0) {
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
  if (IsMessageCenterBubbleVisible())
    button_->SetState(views::CustomButton::STATE_PRESSED);
  else
    button_->SetState(views::CustomButton::STATE_NORMAL);
  // Change the visibility of the buttons here when rich notifications are not
  // enabled. If rich notifications are enabled, the visibility is changed at
  // UpdateAfterLoginStatusChange() since the visibility won't depend on the
  // number of notifications.
  if (!message_center::IsRichNotificationEnabled()) {
    bool is_visible =
        (status_area_widget()->login_status() != user::LOGGED_IN_NONE) &&
        (status_area_widget()->login_status() != user::LOGGED_IN_LOCKED) &&
        (message_center->NotificationCount() > 0);
    SetVisible(is_visible);
  }
  Layout();
  SchedulePaint();
}

bool WebNotificationTray::ClickedOutsideBubble() {
  // Only hide the message center and quiet mode bubble.
  if (!message_center_bubble() && !quiet_mode_bubble())
    return false;
  quiet_mode_bubble_.reset();
  message_center_tray_->HideMessageCenterBubble();
  return true;
}

message_center::MessageCenter* WebNotificationTray::message_center() {
  return message_center_tray_->message_center();
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

message_center::MessagePopupBubble*
WebNotificationTray::GetPopupBubbleForTest() {
  if (!popup_bubble())
    return NULL;
  return static_cast<message_center::MessagePopupBubble*>(
      popup_bubble()->bubble());
}

}  // namespace ash
