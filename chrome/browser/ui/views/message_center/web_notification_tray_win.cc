// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/message_center/web_notification_tray_win.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/views/message_center/notification_bubble_wrapper_win.h"
#include "chrome/browser/ui/views/status_icons/status_icon_win.h"
#include "grit/theme_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/screen.h"
#include "ui/message_center/message_bubble_base.h"
#include "ui/message_center/message_center_bubble.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/message_center/message_popup_bubble.h"
#include "ui/views/widget/widget.h"

namespace {

// Menu commands
const int kToggleQuietMode = 0;
const int kEnableQuietModeHour = 1;
const int kEnableQuietModeDay = 2;

// Tray constants
const int kPaddingFromLeftEdgeOfSystemTrayBottomAlignment = 8;

gfx::Rect GetCornerAnchorRect(gfx::Size preferred_size) {
  // TODO(dewittj): Use the preference to determine which corner to anchor from.
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  gfx::Rect rect = screen->GetPrimaryDisplay().work_area();
  rect.Inset(10, 5);
  gfx::Point bottom_right(
    rect.bottom_right().x() - preferred_size.width() / 2,
    rect.bottom_right().y());
  return gfx::Rect(bottom_right, gfx::Size());
}

// GetMouseAnchorRect returns a rectangle that is near the cursor point, but
// whose behavior depends on where the Windows taskbar is. If it is on the
// top or bottom of the screen, we want the arrow to touch the edge of the
// taskbar directly above or below the mouse pointer and within the work area.
// Otherwise, position the anchor on the mouse cursor directly.
gfx::Rect GetMouseAnchorRect() {
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  gfx::Rect usable_area = screen->GetPrimaryDisplay().bounds();
  gfx::Rect work_area = screen->GetPrimaryDisplay().work_area();

  // Inset the rectangle by the taskbar width if it is on top or bottom.
  usable_area.set_y(work_area.y());
  usable_area.set_height(work_area.height());

  // Keep the anchor from being too close to the edge of the screen.
  usable_area.Inset(kPaddingFromLeftEdgeOfSystemTrayBottomAlignment, 0);

  // Use a mouse point that is on the mouse cursor, unless the mouse is over the
  // start menu and the start menu is on the top or bottom.
  gfx::Point cursor = screen->GetCursorScreenPoint();
  gfx::Rect mouse_anchor_rect(
      gfx::BoundingRect(cursor, usable_area.bottom_right()));
  mouse_anchor_rect.set_height(0);
  if (!usable_area.Contains(cursor))
    mouse_anchor_rect.AdjustToFit(usable_area);
  mouse_anchor_rect.set_width(0);
  return mouse_anchor_rect;
}

gfx::ImageSkia GetIcon(bool has_unread) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* icon = rb.GetImageSkiaNamed(
      has_unread ? IDR_NOTIFICATION_TRAY_LIT : IDR_NOTIFICATION_TRAY_DIM);
  DCHECK(icon);
  return *icon;
}

}  // namespace

namespace message_center {

MessageCenterTrayDelegate* CreateMessageCenterTray() {
  return new WebNotificationTrayWin();
}

WebNotificationTrayWin::WebNotificationTrayWin()
    : status_icon_(NULL),
      message_center_visible_(false) {
  message_center_tray_.reset(new MessageCenterTray(
      this, g_browser_process->message_center()));
  StatusTray* status_tray = g_browser_process->status_tray();
  status_icon_ = status_tray->CreateStatusIcon();
  status_icon_->AddObserver(this);
  status_icon_->SetImage(
      GetIcon(message_center()->UnreadNotificationCount() > 0));

  AddQuietModeMenu(status_icon_);
}

WebNotificationTrayWin::~WebNotificationTrayWin() {
  // Reset this early so that delegated events during destruction don't cause
  // problems.
  message_center_tray_.reset();
  status_icon_->RemoveObserver(this);
  StatusTray* status_tray = g_browser_process->status_tray();
  status_tray->RemoveStatusIcon(status_icon_);
  status_icon_ = NULL;
}

message_center::MessageCenter* WebNotificationTrayWin::message_center() {
  return message_center_tray_->message_center();
}

bool WebNotificationTrayWin::ShowPopups() {
  scoped_ptr<message_center::MessagePopupBubble> bubble(
      new message_center::MessagePopupBubble(message_center()));
  popup_bubble_.reset(new internal::NotificationBubbleWrapperWin(
      this, bubble.Pass(), views::TrayBubbleView::ANCHOR_TYPE_BUBBLE));
  return true;
}

void WebNotificationTrayWin::HidePopups() {
  popup_bubble_.reset();
}

bool WebNotificationTrayWin::ShowMessageCenter() {
  // Calculate the maximum height of the message center, given its anchor.
  scoped_ptr<message_center::MessageCenterBubble> bubble(
      new message_center::MessageCenterBubble(message_center()));
  gfx::Point anchor_center = message_center_anchor_rect_.CenterPoint();
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  gfx::Rect work_area = screen->GetPrimaryDisplay().work_area();
  gfx::Point work_area_center = work_area.CenterPoint();
  const int zMarginFromEdgeOfWorkArea = 10;
  int max_height = 0;
  if (work_area_center < anchor_center)
    max_height = anchor_center.y() - work_area.origin().y();
  else
    max_height = work_area.bottom() - message_center_anchor_rect_.bottom();
  bubble->SetMaxHeight(max_height - zMarginFromEdgeOfWorkArea);

  message_center_bubble_.reset(new internal::NotificationBubbleWrapperWin(
      this,
      bubble.Pass(),
      views::TrayBubbleView::ANCHOR_TYPE_TRAY));
  return true;
}

void WebNotificationTrayWin::HideMessageCenter() {
  message_center_bubble_.reset();
}

void WebNotificationTrayWin::UpdateMessageCenter() {
  if (message_center_bubble_.get())
    message_center_bubble_->bubble()->ScheduleUpdate();
}

void WebNotificationTrayWin::UpdatePopups() {
  if (popup_bubble_.get())
    popup_bubble_->bubble()->ScheduleUpdate();
};

void WebNotificationTrayWin::OnMessageCenterTrayChanged() {
  bool has_unread_notifications =
      message_center()->UnreadNotificationCount() > 0;
  status_icon_->SetImage(GetIcon(has_unread_notifications));
}

gfx::Rect WebNotificationTrayWin::GetAnchorRect(
    gfx::Size preferred_size,
    views::TrayBubbleView::AnchorType anchor_type,
    views::TrayBubbleView::AnchorAlignment anchor_alignment) {
  if (anchor_type == views::TrayBubbleView::ANCHOR_TYPE_TRAY)
    return message_center_anchor_rect_;
  return GetCornerAnchorRect(preferred_size);
}

views::TrayBubbleView::AnchorAlignment
WebNotificationTrayWin::GetAnchorAlignment() {
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  // TODO(dewittj): It's possible GetPrimaryDisplay is wrong.
  gfx::Rect screen_bounds = screen->GetPrimaryDisplay().bounds();
  gfx::Rect work_area = screen->GetPrimaryDisplay().work_area();

  // Comparing the work area to the screen bounds gives us the location of the
  // taskbar.  If the work area is less tall than the screen, assume the taskbar
  // is on the bottom, and cause the arrow to be displayed on the bottom of the
  // bubble.  Otherwise, cause the arrow to be displayed on the side of the
  // bubble that the taskbar is on.
  if (work_area.height() < screen_bounds.height())
    return views::TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM;
  if (work_area.x() > screen_bounds.x())
    return views::TrayBubbleView::ANCHOR_ALIGNMENT_LEFT;
  return views::TrayBubbleView::ANCHOR_ALIGNMENT_RIGHT;
}

gfx::NativeView WebNotificationTrayWin::GetBubbleWindowContainer() {
  return NULL;
}

void WebNotificationTrayWin::OnStatusIconClicked() {
  UpdateAnchorRect();
  message_center_tray_->ToggleMessageCenterBubble();
}

void WebNotificationTrayWin::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {
  if (message_center_bubble_.get() &&
      bubble_view == message_center_bubble_->bubble_view()) {
    message_center_tray_->HideMessageCenterBubble();
  } else if (popup_bubble_.get() &&
      bubble_view == popup_bubble_->bubble_view()) {
    message_center_tray_->HidePopupBubble();
  }
}

bool WebNotificationTrayWin::IsCommandIdChecked(int command_id) const {
  if (command_id != kToggleQuietMode)
    return false;
  return message_center_tray_->message_center()->quiet_mode();
}

bool WebNotificationTrayWin::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool WebNotificationTrayWin::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void WebNotificationTrayWin::ExecuteCommand(int command_id) {
  if (command_id == kToggleQuietMode) {
    bool in_quiet_mode = message_center()->quiet_mode();
    message_center()->notification_list()->SetQuietMode(!in_quiet_mode);
    return;
  }
  base::TimeDelta expires_in = command_id == kEnableQuietModeDay ?
      base::TimeDelta::FromDays(1):
      base::TimeDelta::FromHours(1);
  message_center()->notification_list()->EnterQuietModeWithExpire(expires_in);
}

void WebNotificationTrayWin::UpdateAnchorRect() {
  message_center_anchor_rect_ = GetMouseAnchorRect();
}

void WebNotificationTrayWin::AddQuietModeMenu(StatusIcon* status_icon) {
  DCHECK(status_icon);
  ui::SimpleMenuModel* menu = new ui::SimpleMenuModel(this);

  menu->AddCheckItem(kToggleQuietMode,
                     l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_QUIET_MODE));
  menu->AddItem(kEnableQuietModeHour,
                l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_QUIET_MODE_1HOUR));
  menu->AddItem(kEnableQuietModeDay,
                l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_QUIET_MODE_1DAY));

  status_icon->SetContextMenu(menu);
}

message_center::MessageCenterBubble*
WebNotificationTrayWin::GetMessageCenterBubbleForTest() {
  if (!message_center_bubble_.get())
    return NULL;
  return static_cast<message_center::MessageCenterBubble*>(
      message_center_bubble_->bubble());
}

message_center::MessagePopupBubble*
WebNotificationTrayWin::GetPopupBubbleForTest() {
  if (!popup_bubble_.get())
    return NULL;
  return static_cast<message_center::MessagePopupBubble*>(
      popup_bubble_->bubble());
}

}  // namespace message_center
