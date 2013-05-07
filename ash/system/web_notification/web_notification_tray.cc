// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_tray.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/auto_reset.h"
#include "base/i18n/number_formatting.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "grit/ui_strings.h"
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
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"

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
namespace {

const int kWebNotificationIconSize = 31;

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

  DISALLOW_COPY_AND_ASSIGN(WebNotificationBubbleWrapper);
};

class WebNotificationButton : public views::CustomButton {
 public:
  WebNotificationButton(views::ButtonListener* listener)
      : views::CustomButton(listener),
        is_bubble_visible_(false),
        unread_count_(0) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

    icon_ = new views::ImageView();
    icon_->SetImage(rb.GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_NOTIFY_BUTTON_ICON));
    AddChildView(icon_);

    unread_label_ = new views::Label();
    SetupLabelForTray(unread_label_);
    AddChildView(unread_label_);
    unread_label_->SetVisible(false);
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
    unread_label_->SetText((unread_count > 9) ?
        l10n_util::GetStringUTF16(IDS_ASH_NOTIFICATION_UNREAD_COUNT_NINE_PLUS) :
        base::FormatNumber(unread_count));
    UpdateIconVisibility();
  }

 protected:
  // Overridden from views::ImageButton:
  virtual void Layout() OVERRIDE {
    views::CustomButton::Layout();
    icon_->SetBoundsRect(bounds());
    unread_label_->SetBoundsRect(bounds());
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(kWebNotificationIconSize, kWebNotificationIconSize);
  }

 private:
  void UpdateIconVisibility() {
    if (!is_bubble_visible_ && unread_count_ > 0) {
      icon_->SetVisible(false);
      unread_label_->SetVisible(true);
    } else {
      icon_->SetVisible(true);
      unread_label_->SetVisible(false);
    }
    InvalidateLayout();
    SchedulePaint();
  }

  bool is_bubble_visible_;
  int unread_count_;

  views::ImageView* icon_;
  views::Label* unread_label_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationButton);
};

}  // namespace internal

WebNotificationTray::WebNotificationTray(
    internal::StatusAreaWidget* status_area_widget)
    : TrayBackgroundView(status_area_widget),
      button_(NULL),
      show_message_center_on_unlock_(false),
      should_update_tray_content_(false),
      should_block_shelf_auto_hide_(false) {
  button_ = new internal::WebNotificationButton(this);
  button_->set_triggerable_event_flags(
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON);
  tray_container()->AddChildView(button_);
  SetContentsBackground();
  tray_container()->set_border(NULL);
  SetVisible(false);
  message_center_tray_.reset(new message_center::MessageCenterTray(
      this,
      message_center::MessageCenter::Get()));
  OnMessageCenterTrayChanged();
}

WebNotificationTray::~WebNotificationTray() {
  // Release any child views that might have back pointers before ~View().
  message_center_bubble_.reset();
  popup_bubble_.reset();
  popup_collection_.reset();
}

// Public methods.

bool WebNotificationTray::ShowMessageCenter() {
  if (!ShouldShowMessageCenter())
    return false;

  should_block_shelf_auto_hide_ = true;
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
  button_->SetBubbleVisible(true);
  return true;
}

void WebNotificationTray::HideMessageCenter() {
  if (!message_center_bubble())
    return;
  message_center_bubble_.reset();
  should_block_shelf_auto_hide_ = false;
  show_message_center_on_unlock_ = false;
  status_area_widget()->SetHideSystemNotifications(false);
  GetShelfLayoutManager()->UpdateAutoHideState();
  button_->SetBubbleVisible(false);
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
        ash::Shell::GetContainer(
            GetWidget()->GetNativeView()->GetRootWindow(),
            internal::kShellWindowId_StatusContainer),
        message_center()));
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
};

void WebNotificationTray::HidePopups() {
  popup_bubble_.reset();
  popup_collection_.reset();
}

// Private methods.

bool WebNotificationTray::ShouldShowMessageCenter() {
  return status_area_widget()->login_status() != user::LOGGED_IN_LOCKED &&
      !(status_area_widget()->system_tray() &&
        status_area_widget()->system_tray()->HasNotificationBubble());
}

void WebNotificationTray::ShowQuietModeMenu() {
  base::AutoReset<bool> reset(&should_block_shelf_auto_hide_, true);
  scoped_ptr<ui::MenuModel> menu_model(
      message_center_tray_->CreateQuietModeMenu());
  quiet_mode_menu_runner_.reset(new views::MenuRunner(menu_model.get()));
  gfx::Point point;
  views::View::ConvertPointToScreen(this, &point);
  if (quiet_mode_menu_runner_->RunMenuAt(
      GetWidget(),
      NULL,
      gfx::Rect(point, bounds().size()),
      views::MenuItemView::BUBBLE_ABOVE,
      views::MenuRunner::HAS_MNEMONICS) == views::MenuRunner::MENU_DELETED)
    return;

  quiet_mode_menu_runner_.reset();
  GetShelfLayoutManager()->UpdateAutoHideState();
}

bool WebNotificationTray::ShouldShowQuietModeMenu(const ui::Event& event) {
  // TODO(mukai): Add keyboard event handler.
  if (!event.IsMouseEvent())
    return false;

  const ui::MouseEvent* mouse_event =
      static_cast<const ui::MouseEvent*>(&event);

  return mouse_event->IsRightMouseButton();
}

void WebNotificationTray::UpdateAfterLoginStatusChange(
    user::LoginStatus login_status) {
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
  OnMessageCenterTrayChanged();
}

bool WebNotificationTray::ShouldBlockLauncherAutoHide() const {
  return should_block_shelf_auto_hide_;
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
  tray_container()->set_border(NULL);
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
  if (message_center_bubble()) {
    message_center_bubble()->bubble_view()->UpdateBubble();
    UpdateBubbleViewArrow(message_center_bubble()->bubble_view());
  }
}

base::string16 WebNotificationTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_MESSAGE_CENTER_ACCESSIBLE_NAME);
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
  if (ShouldShowQuietModeMenu(event)) {
    ShowQuietModeMenu();
    return true;
  }

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

base::string16 WebNotificationTray::GetAccessibleNameForBubble() {
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

void WebNotificationTray::OnMessageCenterTrayChanged() {
  // Do not update the tray contents directly. Multiple change events can happen
  // consecutively, and calling Update in the middle of those events will show
  // intermediate unread counts for a moment.
  should_update_tray_content_ = true;
  MessageLoop::current()->PostTask(
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
  SetVisible((status_area_widget()->login_status() != user::LOGGED_IN_NONE) &&
             (status_area_widget()->login_status() != user::LOGGED_IN_LOCKED) &&
             (message_center->NotificationCount() > 0));
  Layout();
  SchedulePaint();
}

bool WebNotificationTray::ClickedOutsideBubble() {
  // Only hide the message center.
  if (!message_center_bubble())
    return false;
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
