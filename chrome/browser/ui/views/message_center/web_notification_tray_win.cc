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
#include "content/public/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/theme_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/message_center/views/message_bubble_base.h"
#include "ui/message_center/views/message_center_bubble.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/views/widget/widget.h"

namespace {

// Tray constants
const int kScreenEdgePadding = 2;

const int kSystemTrayWidth = 16;
const int kSystemTrayHeight = 16;
const int kNumberOfSystemTraySprites = 10;

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

gfx::ImageSkia GetIcon(int unread_count) {
  bool has_unread = unread_count > 0;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (!has_unread)
    return *rb.GetImageSkiaNamed(IDR_NOTIFICATION_TRAY_EMPTY);

  // TODO(dewittj): Use scale factors other than 100P.
  scoped_ptr<gfx::Canvas> canvas(new gfx::Canvas(
      gfx::Size(kSystemTrayWidth, kSystemTrayHeight),
      ui::SCALE_FACTOR_100P,
      false));

  // Draw the attention-grabbing background image.
  canvas->DrawImageInt(
      *rb.GetImageSkiaNamed(IDR_NOTIFICATION_TRAY_ATTENTION), 0, 0);

  // |numbers| is a sprite map with the image of a number from 1-9 and 9+. They
  // are arranged horizontally, and have a transparent background.
  gfx::ImageSkia* numbers = rb.GetImageSkiaNamed(IDR_NOTIFICATION_TRAY_NUMBERS);

  // Assume that the last sprite is the catch-all for higher numbers of
  // notifications.
  int effective_unread = std::min(unread_count, kNumberOfSystemTraySprites);
  int x_offset = (effective_unread - 1) * kSystemTrayWidth;

  canvas->DrawImageInt(*numbers,
                       x_offset, 0, kSystemTrayWidth, kSystemTrayHeight,
                       0, 0, kSystemTrayWidth, kSystemTrayHeight,
                       false);

  return gfx::ImageSkia(canvas->ExtractImageRep());
}

}  // namespace

using content::UserMetricsAction;

namespace message_center {

MessageCenterTrayDelegate* CreateMessageCenterTray() {
  return new WebNotificationTrayWin();
}

WebNotificationTrayWin::WebNotificationTrayWin()
    : status_icon_(NULL),
      message_center_visible_(false),
      should_update_tray_content_(true) {
  message_center_tray_.reset(new MessageCenterTray(
      this, g_browser_process->message_center()));
  UpdateStatusIcon();
}

WebNotificationTrayWin::~WebNotificationTrayWin() {
  // Reset this early so that delegated events during destruction don't cause
  // problems.
  message_center_tray_.reset();
  DestroyStatusIcon();
}

message_center::MessageCenter* WebNotificationTrayWin::message_center() {
  return message_center_tray_->message_center();
}

bool WebNotificationTrayWin::ShowPopups() {
  popup_collection_.reset(
      new message_center::MessagePopupCollection(NULL, message_center()));
  return true;
}

void WebNotificationTrayWin::HidePopups() {
  popup_collection_.reset();
}

bool WebNotificationTrayWin::ShowMessageCenter() {
  content::RecordAction(UserMetricsAction("Notifications.ShowMessageCenter"));

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

void WebNotificationTrayWin::UpdatePopups() {
  // |popup_collection_| receives notification add/remove events and updates
  // itself, so this method doesn't need to do anything.
  // TODO(mukai): remove this method (currently this is used by
  // non-rich-notifications in ChromeOS).
};

void WebNotificationTrayWin::OnMessageCenterTrayChanged() {
  // See the comments in ash/system/web_notification/web_notification_tray.cc
  // for why PostTask.
  should_update_tray_content_ = true;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&WebNotificationTrayWin::UpdateStatusIcon, AsWeakPtr()));
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
  if (!should_update_tray_content_)
    return;
  should_update_tray_content_ = false;

  int total_notifications = message_center()->NotificationCount();
  if (total_notifications == 0) {
    DestroyStatusIcon();
    return;
  }

  int unread_notifications = message_center()->UnreadNotificationCount();
  StatusIcon* status_icon = GetStatusIcon();
  if (!status_icon)
    return;

  status_icon->SetImage(GetIcon(unread_notifications));

  string16 product_name(l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
  if (unread_notifications > 0) {
    string16 str_unread_count = base::FormatNumber(unread_notifications);
    status_icon->SetToolTip(l10n_util::GetStringFUTF16(
        IDS_MESSAGE_CENTER_TOOLTIP_UNREAD, product_name, str_unread_count));
  } else {
    status_icon->SetToolTip(l10n_util::GetStringFUTF16(
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
  }
}

StatusIcon* WebNotificationTrayWin::GetStatusIcon() {
  if (status_icon_)
    return status_icon_;

  StatusTray* status_tray = g_browser_process->status_tray();
  if (!status_tray)
    return NULL;

  StatusIcon* status_icon = status_tray->CreateStatusIcon();
  if (!status_icon)
    return NULL;

  status_icon_ = status_icon;
  status_icon_->AddObserver(this);
  AddQuietModeMenu(status_icon_);

  return status_icon_;
}

void WebNotificationTrayWin::DestroyStatusIcon() {
  if (!status_icon_)
    return;

  status_icon_->RemoveObserver(this);
  StatusTray* status_tray = g_browser_process->status_tray();
  if (status_tray)
    status_tray->RemoveStatusIcon(status_icon_);
  status_icon_ = NULL;
}

void WebNotificationTrayWin::AddQuietModeMenu(StatusIcon* status_icon) {
  DCHECK(status_icon);
  status_icon->SetContextMenu(message_center_tray_->CreateQuietModeMenu());
}

message_center::MessageCenterBubble*
WebNotificationTrayWin::GetMessageCenterBubbleForTest() {
  if (!message_center_bubble_.get())
    return NULL;
  return static_cast<message_center::MessageCenterBubble*>(
      message_center_bubble_->bubble());
}

}  // namespace message_center
