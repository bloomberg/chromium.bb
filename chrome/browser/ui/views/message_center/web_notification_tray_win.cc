// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/message_center/web_notification_tray_win.h"

#include "base/i18n/number_formatting.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/views/message_center/notification_bubble_wrapper_win.h"
#include "chrome/browser/ui/views/status_icons/status_icon_win.h"
#include "grit/chromium_strings.h"
#include "grit/theme_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/screen.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/message_center/views/message_bubble_base.h"
#include "ui/message_center/views/message_center_bubble.h"
#include "ui/message_center/views/message_popup_bubble.h"
#include "ui/views/widget/widget.h"

namespace {

// Menu commands
const int kToggleQuietMode = 0;
const int kEnableQuietModeHour = 1;
const int kEnableQuietModeDay = 2;

// Tray constants
const int kScreenEdgePadding = 2;

gfx::Rect GetCornerAnchorRect() {
  // TODO(dewittj): Use the preference to determine which corner to anchor from.
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  gfx::Rect rect = screen->GetPrimaryDisplay().work_area();
  rect.Inset(kScreenEdgePadding, kScreenEdgePadding);
  return gfx::Rect(rect.bottom_right(), gfx::Size());
}

gfx::Point GetClosestCorner(gfx::Rect rect, gfx::Point query) {
  gfx::Point center_point = rect.CenterPoint();
  gfx::Point rv;

  if (query.x() > center_point.x())
    rv.set_x(rect.right());
  else
    rv.set_x(rect.x());

  if (query.y() > center_point.y())
    rv.set_y(rect.bottom());
  else
    rv.set_y(rect.y());

  return rv;
}

// GetMouseAnchorRect returns a rectangle that has one corner where the mouse
// clicked, and the opposite corner at the closest corner of the work area
// (inset by an appropriate margin.)
gfx::Rect GetMouseAnchorRect(gfx::Point cursor) {
  // TODO(dewittj): GetNativeScreen could be wrong for Aura.
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  gfx::Rect work_area = screen->GetPrimaryDisplay().work_area();
  work_area.Inset(kScreenEdgePadding, kScreenEdgePadding);
  gfx::Point corner = GetClosestCorner(work_area, cursor);

  gfx::Rect mouse_anchor_rect(gfx::BoundingRect(cursor, corner));
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

  UpdateStatusIcon();
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
      this,
      bubble.Pass(),
      internal::NotificationBubbleWrapperWin::BUBBLE_TYPE_POPUP));
  return true;
}

void WebNotificationTrayWin::HidePopups() {
  popup_bubble_.reset();
}

bool WebNotificationTrayWin::ShowMessageCenter() {
  scoped_ptr<message_center::MessageCenterBubble> bubble(
      new message_center::MessageCenterBubble(message_center()));
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  gfx::Rect work_area = screen->GetPrimaryDisplay().work_area();
  views::TrayBubbleView::AnchorAlignment alignment = GetAnchorAlignment();

  int max_height = work_area.height();

  // If the alignment is left- or right-oriented, the bubble can fill up the
  // entire vertical height of the screen since the bubble is rendered to the
  // side of the clicked icon.  Otherwise we have to adjust for the arrow's
  // height.
  if (alignment == views::TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM ||
      alignment == views::TrayBubbleView::ANCHOR_ALIGNMENT_TOP) {
    max_height -= 2*kScreenEdgePadding;

    // If the work area contains the click point, then we know that the icon is
    // not in the taskbar.  Then we need to subtract the distance of the click
    // point from the edge of the work area so we can see the whole bubble.
    if (work_area.Contains(mouse_click_point_)) {
      max_height -= std::min(mouse_click_point_.y() - work_area.y(),
                             work_area.bottom() - mouse_click_point_.y());
    }
  }
  bubble->SetMaxHeight(max_height);

  message_center_bubble_.reset(new internal::NotificationBubbleWrapperWin(
      this,
      bubble.Pass(),
      internal::NotificationBubbleWrapperWin::BUBBLE_TYPE_MESSAGE_CENTER));
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
  UpdateStatusIcon();
}

gfx::Rect WebNotificationTrayWin::GetMessageCenterAnchor() {
  return GetMouseAnchorRect(mouse_click_point_);
}

gfx::Rect WebNotificationTrayWin::GetPopupAnchor() {
  return GetCornerAnchorRect();
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
  if (work_area.width() < screen_bounds.width()) {
    if (work_area.x() > screen_bounds.x())
      return views::TrayBubbleView::ANCHOR_ALIGNMENT_LEFT;
    return views::TrayBubbleView::ANCHOR_ALIGNMENT_RIGHT;
  }
  return views::TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM;
}

gfx::NativeView WebNotificationTrayWin::GetBubbleWindowContainer() {
  return NULL;
}

void WebNotificationTrayWin::UpdateStatusIcon() {
  int unread_notifications = message_center()->UnreadNotificationCount();
  status_icon_->SetImage(GetIcon(unread_notifications > 0));

  string16 product_name(l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
  if (unread_notifications > 0) {
    string16 str_unread_count = base::FormatNumber(unread_notifications);
    status_icon_->SetToolTip(l10n_util::GetStringFUTF16(
        IDS_MESSAGE_CENTER_TOOLTIP_UNREAD, product_name, str_unread_count));
  } else {
    status_icon_->SetToolTip(l10n_util::GetStringFUTF16(
        IDS_MESSAGE_CENTER_TOOLTIP, product_name));
  }
}

void WebNotificationTrayWin::OnStatusIconClicked() {
  // TODO(dewittj): It's possible GetNativeScreen is wrong for win-aura.
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  mouse_click_point_ = screen->GetCursorScreenPoint();
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
