// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_tray.h"

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/web_notification/ash_popup_alignment_delegate.h"
#include "base/auto_reset.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "grit/ui_strings.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/screen.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/message_center/views/message_bubble_base.h"
#include "ui/message_center/views/message_center_bubble.h"
#include "ui/message_center/views/message_popup_collection.h"
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
    bubble_view->SetArrowPaintType(views::BubbleBorder::PAINT_NONE);
    bubble_wrapper_.reset(new TrayBubbleWrapper(tray, bubble_view));
    bubble->InitializeContents(bubble_view);
  }

  message_center::MessageBubbleBase* bubble() const { return bubble_.get(); }

  // Convenience accessors.
  views::TrayBubbleView* bubble_view() const { return bubble_->bubble_view(); }

 private:
  scoped_ptr<message_center::MessageBubbleBase> bubble_;
  scoped_ptr<TrayBubbleWrapper> bubble_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationBubbleWrapper);
};

class WebNotificationButton : public views::CustomButton {
 public:
  WebNotificationButton(views::ButtonListener* listener)
      : views::CustomButton(listener),
        is_bubble_visible_(false),
        unread_count_(0) {
    SetLayoutManager(new views::FillLayout);
    unread_label_ = new views::Label();
    SetupLabelForTray(unread_label_);
    AddChildView(unread_label_);
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
    // TODO(mukai): move NINE_PLUS message to ui_strings, it doesn't need to be
    // in ash_strings.
    unread_label_->SetText((unread_count > 9) ?
        l10n_util::GetStringUTF16(IDS_ASH_NOTIFICATION_UNREAD_COUNT_NINE_PLUS) :
        base::FormatNumber(unread_count));
    UpdateIconVisibility();
  }

 protected:
  // Overridden from views::ImageButton:
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return gfx::Size(kShelfItemHeight, kShelfItemHeight);
  }

  virtual int GetHeightForWidth(int width) const OVERRIDE {
    return GetPreferredSize().height();
  }

 private:
  void UpdateIconVisibility() {
    unread_label_->SetEnabledColor(
        (!is_bubble_visible_ && unread_count_ > 0) ?
        kWebNotificationColorWithUnread : kWebNotificationColorNoUnread);
    SchedulePaint();
  }

  bool is_bubble_visible_;
  int unread_count_;

  views::Label* unread_label_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationButton);
};

WebNotificationTray::WebNotificationTray(StatusAreaWidget* status_area_widget)
    : TrayBackgroundView(status_area_widget),
      button_(NULL),
      show_message_center_on_unlock_(false),
      should_update_tray_content_(false),
      should_block_shelf_auto_hide_(false) {
  button_ = new WebNotificationButton(this);
  button_->set_triggerable_event_flags(
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON);
  tray_container()->AddChildView(button_);
  SetContentsBackground();
  tray_container()->SetBorder(views::Border::NullBorder());
  message_center_tray_.reset(new message_center::MessageCenterTray(
      this,
      message_center::MessageCenter::Get()));
  popup_alignment_delegate_.reset(new AshPopupAlignmentDelegate());
  popup_collection_.reset(new message_center::MessagePopupCollection(
      ash::Shell::GetContainer(
          status_area_widget->GetNativeView()->GetRootWindow(),
          kShellWindowId_StatusContainer),
      message_center(),
      message_center_tray_.get(),
      popup_alignment_delegate_.get()));
  const gfx::Display& display = Shell::GetScreen()->GetDisplayNearestWindow(
      status_area_widget->GetNativeView());
  popup_alignment_delegate_->StartObserving(Shell::GetScreen(), display);
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
      new message_center::MessageCenterBubble(
          message_center(),
          message_center_tray_.get(),
          true);

  int max_height = 0;
  aura::Window* status_area_window = status_area_widget()->GetNativeView();
  switch (GetShelfLayoutManager()->GetAlignment()) {
    case SHELF_ALIGNMENT_BOTTOM: {
      gfx::Rect shelf_bounds = GetShelfLayoutManager()->GetIdealBounds();
      max_height = shelf_bounds.y();
      break;
    }
    case SHELF_ALIGNMENT_TOP: {
      aura::Window* root = status_area_window->GetRootWindow();
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

  message_center_bubble->SetMaxHeight(std::max(0,
                                               max_height - kTraySpacing));
  if (show_settings)
    message_center_bubble->SetSettingsVisible();
  message_center_bubble_.reset(
      new WebNotificationBubbleWrapper(this, message_center_bubble));

  status_area_widget()->SetHideSystemNotifications(true);
  GetShelfLayoutManager()->UpdateAutoHideState();
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
  status_area_widget()->SetHideSystemNotifications(false);
  GetShelfLayoutManager()->UpdateAutoHideState();
  button_->SetBubbleVisible(false);
}

void WebNotificationTray::SetSystemTrayHeight(int height) {
  popup_alignment_delegate_->SetSystemTrayHeight(height);
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
  return status_area_widget()->login_status() != user::LOGGED_IN_LOCKED &&
      !(status_area_widget()->system_tray() &&
        status_area_widget()->system_tray()->HasNotificationBubble());
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
    user::LoginStatus login_status) {
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
  return l10n_util::GetStringUTF16(
      IDS_MESSAGE_CENTER_ACCESSIBLE_NAME);
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

void WebNotificationTray::HideBubble(const views::TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

bool WebNotificationTray::ShowNotifierSettings() {
  if (message_center_bubble()) {
    static_cast<message_center::MessageCenterBubble*>(
        message_center_bubble()->bubble())->SetSettingsVisible();
    return true;
  }
  return ShowMessageCenterInternal(true /* show_settings */);
}

bool WebNotificationTray::IsContextMenuEnabled() const {
  user::LoginStatus login_status = status_area_widget()->login_status();
  return login_status != user::LOGGED_IN_NONE
      && login_status != user::LOGGED_IN_LOCKED;
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
  base::TimeDelta expires_in = command_id == kEnableQuietModeDay ?
      base::TimeDelta::FromDays(1):
      base::TimeDelta::FromHours(1);
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
  base::MessageLoop::current()->PostTask(
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
  // Only hide the message center
  if (!message_center_bubble())
    return false;

  message_center_tray_->HideMessageCenterBubble();
  return true;
}

message_center::MessageCenter* WebNotificationTray::message_center() const {
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

}  // namespace ash
