// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_tray.h"

#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "ash/system/web_notification/message_center_bubble.h"
#include "ash/system/web_notification/popup_bubble.h"
#include "ash/system/web_notification/web_notification.h"
#include "ash/system/web_notification/web_notification_bubble.h"
#include "ash/system/web_notification/web_notification_contents_view.h"
#include "ash/system/web_notification/web_notification_list.h"
#include "ash/system/web_notification/web_notification_view.h"
#include "ash/wm/shelf_layout_manager.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget_observer.h"

namespace {

// Tray constants
const int kTrayContainerVerticalPaddingBottomAlignment = 3;
const int kTrayContainerHorizontalPaddingBottomAlignment = 1;
const int kTrayContainerVerticalPaddingVerticalAlignment = 1;
const int kTrayContainerHorizontalPaddingVerticalAlignment = 0;
const int kPaddingFromLeftEdgeOfSystemTrayBottomAlignment = 8;
const int kPaddingFromTopEdgeOfSystemTrayVerticalAlignment = 10;

std::string GetNotificationText(int notification_count) {
  if (notification_count >= 100)
    return "99+";
  return base::StringPrintf("%d", notification_count);
}

}  // namespace

namespace ash {

// WebNotificationTray statics (for unit tests)

// Limit the number of visible notifications.
const size_t WebNotificationTray::kMaxVisibleTrayNotifications = 100;
const size_t WebNotificationTray::kMaxVisiblePopupNotifications = 5;

using message_center::MessageCenterBubble;
using message_center::PopupBubble;
using message_center::WebNotification;
using message_center::WebNotificationBubble;
using message_center::WebNotificationList;
using message_center::WebNotificationView;

WebNotificationTray::WebNotificationTray(
    internal::StatusAreaWidget* status_area_widget)
    : internal::TrayBackgroundView(status_area_widget),
      notification_list_(new WebNotificationList()),
      button_(NULL),
      delegate_(NULL),
      show_message_center_on_unlock_(false) {
  button_ = new views::ImageButton(this);

  tray_container()->AddChildView(button_);

  UpdateTray();
}

WebNotificationTray::~WebNotificationTray() {
  // Release any child views that might have back pointers before ~View().
  notification_list_.reset();
  message_center_bubble_.reset();
  popup_bubble_.reset();
}

void WebNotificationTray::SetDelegate(Delegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
}

// Add/Update/RemoveNotification are called by the client code, i.e the
// Delegate implementation or its proxy.

void WebNotificationTray::AddNotification(const std::string& id,
                                          const string16& title,
                                          const string16& message,
                                          const string16& display_source,
                                          const std::string& extension_id) {
  notification_list_->AddNotification(
      id, title, message, display_source, extension_id);
  UpdateTrayAndBubble();
  ShowPopupBubble();
}

void WebNotificationTray::UpdateNotification(const std::string& old_id,
                                             const std::string& new_id,
                                             const string16& title,
                                             const string16& message) {
  notification_list_->UpdateNotificationMessage(old_id, new_id, title, message);
  UpdateTrayAndBubble();
  ShowPopupBubble();
}

void WebNotificationTray::RemoveNotification(const std::string& id) {
  if (!notification_list_->RemoveNotification(id))
    return;
  if (!notification_list_->HasPopupNotifications())
    HidePopupBubble();
  UpdateTrayAndBubble();
}

void WebNotificationTray::SetNotificationImage(const std::string& id,
                                               const gfx::ImageSkia& image) {
  if (!notification_list_->SetNotificationImage(id, image))
    return;
  UpdateTrayAndBubble();
  ShowPopupBubble();
}

void WebNotificationTray::ShowMessageCenterBubble() {
  if (status_area_widget()->login_status() == user::LOGGED_IN_LOCKED)
    return;
  if (message_center_bubble()) {
    UpdateTray();
    return;
  }
  // Indicate that the message center is visible. Clears the unread count.
  notification_list_->SetMessageCenterVisible(true);
  UpdateTray();
  HidePopupBubble();
  message_center_bubble_.reset(new MessageCenterBubble(this));
  status_area_widget()->SetHideSystemNotifications(true);
  Shell::GetInstance()->shelf()->UpdateAutoHideState();
}

void WebNotificationTray::HideMessageCenterBubble() {
  if (!message_center_bubble())
    return;
  message_center_bubble_.reset();
  show_message_center_on_unlock_ = false;
  notification_list_->SetMessageCenterVisible(false);
  UpdateTray();
  status_area_widget()->SetHideSystemNotifications(false);
  Shell::GetInstance()->shelf()->UpdateAutoHideState();
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
    popup_bubble()->ScheduleUpdate();
  } else if (notification_list_->HasPopupNotifications()) {
    popup_bubble_.reset(new PopupBubble(this));
  }
}

void WebNotificationTray::HidePopupBubble() {
  popup_bubble_.reset();
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
  UpdateTray();
}

bool WebNotificationTray::IsMessageCenterBubbleVisible() const {
  return (message_center_bubble() && message_center_bubble_->IsVisible());
}

bool WebNotificationTray::IsMouseInNotificationBubble() const {
  if (!popup_bubble())
    return false;
  return popup_bubble_->bubble_view()->GetBoundsInScreen().Contains(
      gfx::Screen::GetCursorScreenPoint());
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
  }
  if (message_center_bubble_.get())
    message_center_bubble_->bubble_view()->UpdateBubble();
}

string16 WebNotificationTray::GetAccessibleName() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_WEB_NOTIFICATION_TRAY_ACCESSIBLE_NAME);
}

// Private methods invoked by WebNotificationBubble and its child classes

void WebNotificationTray::SendRemoveNotification(const std::string& id) {
  // If this is the only notification in the list, close the bubble.
  if (notification_list_->notifications().size() == 1 &&
      notification_list_->HasNotification(id)) {
    HideMessageCenterBubble();
  }
  if (delegate_)
    delegate_->NotificationRemoved(id);
}

void WebNotificationTray::SendRemoveAllNotifications() {
  HideMessageCenterBubble();
  if (delegate_) {
    const WebNotificationList::Notifications& notifications =
        notification_list_->notifications();
    for (WebNotificationList::Notifications::const_iterator loopiter =
             notifications.begin();
         loopiter != notifications.end(); ) {
      WebNotificationList::Notifications::const_iterator curiter = loopiter++;
      std::string notification_id = curiter->id;
      // May call RemoveNotification and erase curiter.
      delegate_->NotificationRemoved(notification_id);
    }
  }
}

// When we disable notifications, we remove any existing matching
// notifications to avoid adding complicated UI to re-enable the source.
void WebNotificationTray::DisableByExtension(const std::string& id) {
  if (delegate_)
    delegate_->DisableExtension(id);
  // Will call SendRemoveNotification for each matching notification.
  notification_list_->SendRemoveNotificationsByExtension(this, id);
}

void WebNotificationTray::DisableByUrl(const std::string& id) {
  if (delegate_)
    delegate_->DisableNotificationsFromSource(id);
  // Will call SendRemoveNotification for each matching notification.
  notification_list_->SendRemoveNotificationsBySource(this, id);
}

bool WebNotificationTray::PerformAction(const ui::Event& event) {
  ToggleMessageCenterBubble();
  return true;
}

void WebNotificationTray::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  DCHECK(sender == button_);
  ToggleMessageCenterBubble();
}

void WebNotificationTray::ShowSettings(const std::string& id) {
  if (delegate_)
    delegate_->ShowSettings(id);
}

void WebNotificationTray::OnClicked(const std::string& id) {
  if (delegate_)
    delegate_->OnClicked(id);
}

// Other private methods

void WebNotificationTray::ToggleMessageCenterBubble() {
  if (message_center_bubble())
    HideMessageCenterBubble();
  else
    ShowMessageCenterBubble();
  UpdateTray();
}

void WebNotificationTray::UpdateTray() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (notification_list()->unread_count() > 0) {
    button_->SetImage(views::CustomButton::BS_NORMAL, rb.GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_NOTIFY_BUTTON_ACTIVE_NORMAL));
    button_->SetImage(views::CustomButton::BS_HOT, rb.GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_NOTIFY_BUTTON_ACTIVE_HOVER));
    button_->SetImage(views::CustomButton::BS_PUSHED, rb.GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_NOTIFY_BUTTON_ACTIVE_PRESSED));
  } else {
    button_->SetImage(views::CustomButton::BS_NORMAL, rb.GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_NOTIFY_BUTTON_INACTIVE_NORMAL));
    button_->SetImage(views::CustomButton::BS_HOT, rb.GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_NOTIFY_BUTTON_INACTIVE_HOVER));
    button_->SetImage(views::CustomButton::BS_PUSHED, rb.GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_NOTIFY_BUTTON_INACTIVE_PRESSED));
  }
  if (message_center_bubble())
    button_->SetState(views::CustomButton::BS_PUSHED);
  else
    button_->SetState(views::CustomButton::BS_NORMAL);
  bool is_visible =
      (status_area_widget()->login_status() != user::LOGGED_IN_NONE) &&
      (status_area_widget()->login_status() != user::LOGGED_IN_LOCKED) &&
      (!notification_list()->notifications().empty());
  SetVisible(is_visible);
  Layout();
  SchedulePaint();
}

void WebNotificationTray::UpdateTrayAndBubble() {
  if (message_center_bubble()) {
    if (notification_list_->notifications().size() == 0)
      HideMessageCenterBubble();
    else
      message_center_bubble()->ScheduleUpdate();
  }
  if (popup_bubble()) {
    if (notification_list_->notifications().size() == 0)
      HidePopupBubble();
    else
      popup_bubble()->ScheduleUpdate();
  }
  UpdateTray();
}

void WebNotificationTray::HideBubble(WebNotificationBubble* bubble) {
  if (bubble == message_center_bubble()) {
    HideMessageCenterBubble();
  } else if (bubble == popup_bubble()) {
    HidePopupBubble();
  }
}

// Methods for testing

size_t WebNotificationTray::GetNotificationCountForTest() const {
  return notification_list_->notifications().size();
}

bool WebNotificationTray::HasNotificationForTest(const std::string& id) const {
  return notification_list_->HasNotification(id);
}

void WebNotificationTray::RemoveAllNotificationsForTest() {
  notification_list_->RemoveAllNotifications();
}

size_t WebNotificationTray::GetMessageCenterNotificationCountForTest() const {
  if (!message_center_bubble())
    return 0;
  return message_center_bubble()->NumMessageViewsForTest();
}

size_t WebNotificationTray::GetPopupNotificationCountForTest() const {
  if (!popup_bubble())
    return 0;
  return popup_bubble()->NumMessageViewsForTest();
}

}  // namespace ash
