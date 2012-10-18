// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_tray.h"

#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "ash/system/web_notification/message_center_bubble.h"
#include "ash/system/web_notification/popup_bubble.h"
#include "ash/system/web_notification/web_notification.h"
#include "ash/system/web_notification/web_notification_bubble.h"
#include "ash/system/web_notification/web_notification_list.h"
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

using message_center::MessageCenterBubble;
using message_center::PopupBubble;
using message_center::TrayBubbleView;
using message_center::WebNotification;
using message_center::WebNotificationBubble;
using message_center::WebNotificationList;

namespace ash {

namespace internal {

// Class to initialize and manage the WebNotificationBubble and
// TrayBubbleWrapper instances for a bubble.

class WebNotificationBubbleWrapper {
 public:
  // Takes ownership of |bubble| and creates |bubble_wrapper_|.
  WebNotificationBubbleWrapper(WebNotificationTray* tray,
                               WebNotificationBubble* bubble) {
    bubble_.reset(bubble);
    TrayBubbleView::AnchorAlignment anchor_alignment =
        tray->GetAnchorAlignment();
    TrayBubbleView::InitParams init_params =
        bubble->GetInitParams(anchor_alignment);
    views::View* anchor = tray->tray_container();
    if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM) {
      gfx::Point bounds(anchor->width() / 2, 0);
      views::View::ConvertPointToWidget(anchor, &bounds);
      init_params.arrow_offset = bounds.x();
    }
    TrayBubbleView* bubble_view = TrayBubbleView::Create(
        tray->GetBubbleWindowContainer(), anchor, tray, &init_params);
    bubble_wrapper_.reset(new TrayBubbleWrapper(tray, bubble_view));
    bubble->InitializeContents(bubble_view);
  }

  WebNotificationBubble* bubble() const { return bubble_.get(); }

  // Convenience accessors.
  TrayBubbleView* bubble_view() const { return bubble_->bubble_view(); }

 private:
  scoped_ptr<WebNotificationBubble> bubble_;
  scoped_ptr<internal::TrayBubbleWrapper> bubble_wrapper_;
};

}  // namespace internal

WebNotificationTray::WebNotificationTray(
    internal::StatusAreaWidget* status_area_widget)
    : internal::TrayBackgroundView(status_area_widget),
      button_(NULL),
      delegate_(NULL),
      show_message_center_on_unlock_(false) {
  notification_list_.reset(new WebNotificationList(this));

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
  MessageCenterBubble* bubble = new MessageCenterBubble(this);
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
  notification_list_->SetMessageCenterVisible(false);
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
  } else if (notification_list_->HasPopupNotifications()) {
    popup_bubble_.reset(
        new internal::WebNotificationBubbleWrapper(
            this, new PopupBubble(this)));
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
  }
  if (message_center_bubble_.get())
    message_center_bubble_->bubble_view()->UpdateBubble();
}

string16 WebNotificationTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_WEB_NOTIFICATION_TRAY_ACCESSIBLE_NAME);
}

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
void WebNotificationTray::DisableNotificationByExtension(
    const std::string& id) {
  if (delegate_)
    delegate_->DisableExtension(id);
  // Will call SendRemoveNotification for each matching notification.
  notification_list_->SendRemoveNotificationsByExtension(id);
}

void WebNotificationTray::DisableNotificationByUrl(const std::string& id) {
  if (delegate_)
    delegate_->DisableNotificationsFromSource(id);
  // Will call SendRemoveNotification for each matching notification.
  notification_list_->SendRemoveNotificationsBySource(id);
}

void WebNotificationTray::ShowNotificationSettings(const std::string& id) {
  if (delegate_)
    delegate_->ShowSettings(id);
}

void WebNotificationTray::OnNotificationClicked(const std::string& id) {
  if (delegate_)
    delegate_->OnClicked(id);
}

WebNotificationList* WebNotificationTray::GetNotificationList() {
  return notification_list_.get();
}

void WebNotificationTray::HideBubbleWithView(
    const TrayBubbleView* bubble_view) {
  if (message_center_bubble() &&
      bubble_view == message_center_bubble()->bubble_view()) {
    HideMessageCenterBubble();
  } else if (popup_bubble() && bubble_view == popup_bubble()->bubble_view()) {
    HidePopupBubble();
  }
}

bool WebNotificationTray::PerformAction(const ui::Event& event) {
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

void WebNotificationTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void WebNotificationTray::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  DCHECK(sender == button_);
  ToggleMessageCenterBubble();
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
      message_center_bubble()->bubble()->ScheduleUpdate();
  }
  if (popup_bubble()) {
    if (notification_list_->notifications().size() == 0)
      HidePopupBubble();
    else
      popup_bubble()->bubble()->ScheduleUpdate();
  }
  UpdateTray();
}

bool WebNotificationTray::ClickedOutsideBubble() {
  // Only hide the message center.
  if (!message_center_bubble())
    return false;
  HideMessageCenterBubble();
  return true;
}

// Methods for testing

MessageCenterBubble* WebNotificationTray::GetMessageCenterBubbleForTest() {
  if (!message_center_bubble_.get())
    return NULL;
  return static_cast<MessageCenterBubble*>(message_center_bubble_->bubble());
}

PopupBubble* WebNotificationTray::GetPopupBubbleForTest() {
  if (!popup_bubble_.get())
    return NULL;
  return static_cast<PopupBubble*>(popup_bubble_->bubble());
}

}  // namespace ash
