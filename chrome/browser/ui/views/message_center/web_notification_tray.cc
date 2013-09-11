// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/message_center/web_notification_tray.h"

#include "base/i18n/number_formatting.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "content/public/browser/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/theme_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/views/widget/widget.h"

namespace {

// Tray constants
const int kScreenEdgePadding = 2;

const int kSystemTrayWidth = 16;
const int kSystemTrayHeight = 16;
const int kNumberOfSystemTraySprites = 10;

// Number of pixels the message center is offset from the mouse.
const int kMouseOffset = 5;

// Menu commands
const int kToggleQuietMode = 0;
const int kEnableQuietModeHour = 1;
const int kEnableQuietModeDay = 2;

gfx::ImageSkia* GetIcon(int unread_count, bool is_quiet_mode) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  int resource_id = IDR_NOTIFICATION_TRAY_EMPTY;

  if (unread_count) {
    if (is_quiet_mode)
      resource_id = IDR_NOTIFICATION_TRAY_DO_NOT_DISTURB_ATTENTION;
    else
      resource_id = IDR_NOTIFICATION_TRAY_ATTENTION;
  } else if (is_quiet_mode) {
    resource_id = IDR_NOTIFICATION_TRAY_DO_NOT_DISTURB_EMPTY;
  }

  return rb.GetImageSkiaNamed(resource_id);
}

}  // namespace

namespace message_center {

namespace internal {

// Gets the position of the taskbar from the work area bounds. Returns
// ALIGNMENT_NONE if position cannot be found.
Alignment GetTaskbarAlignment() {
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  // TODO(dewittj): It's possible GetPrimaryDisplay is wrong.
  gfx::Rect screen_bounds = screen->GetPrimaryDisplay().bounds();
  gfx::Rect work_area = screen->GetPrimaryDisplay().work_area();
  work_area.Inset(kScreenEdgePadding, kScreenEdgePadding);

  // Comparing the work area to the screen bounds gives us the location of the
  // taskbar.  If the work area is exactly the same as the screen bounds,
  // we are unable to locate the taskbar so we say we don't know it's alignment.
  if (work_area.height() < screen_bounds.height()) {
    if (work_area.y() > screen_bounds.y())
      return ALIGNMENT_TOP;
    return ALIGNMENT_BOTTOM;
  }
  if (work_area.width() < screen_bounds.width()) {
    if (work_area.x() > screen_bounds.x())
      return ALIGNMENT_LEFT;
    return ALIGNMENT_RIGHT;
  }

  return ALIGNMENT_NONE;
}

gfx::Point GetClosestCorner(const gfx::Rect& rect, const gfx::Point& query) {
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

// Gets the corner of the screen where the message center should pop up.
Alignment GetAnchorAlignment(const gfx::Rect& work_area, gfx::Point corner) {
  gfx::Point center = work_area.CenterPoint();

  Alignment anchor_alignment =
      center.y() > corner.y() ? ALIGNMENT_TOP : ALIGNMENT_BOTTOM;
  anchor_alignment =
      (Alignment)(anchor_alignment |
                  (center.x() > corner.x() ? ALIGNMENT_LEFT : ALIGNMENT_RIGHT));

  return anchor_alignment;
}

}  // namespace internal

MessageCenterTrayDelegate* CreateMessageCenterTray() {
  return new WebNotificationTray();
}

WebNotificationTray::WebNotificationTray()
    : message_center_delegate_(NULL),
      status_icon_(NULL),
      status_icon_menu_(NULL),
      message_center_visible_(false),
      should_update_tray_content_(true) {
  message_center_tray_.reset(
      new MessageCenterTray(this, g_browser_process->message_center()));
  last_quiet_mode_state_ = message_center()->IsQuietMode();
}

WebNotificationTray::~WebNotificationTray() {
  // Reset this early so that delegated events during destruction don't cause
  // problems.
  message_center_tray_.reset();
  DestroyStatusIcon();
}

message_center::MessageCenter* WebNotificationTray::message_center() {
  return message_center_tray_->message_center();
}

bool WebNotificationTray::ShowPopups() {
  popup_collection_.reset(new message_center::MessagePopupCollection(
      NULL, message_center(), message_center_tray_.get(), false));
  return true;
}

void WebNotificationTray::HidePopups() { popup_collection_.reset(); }

bool WebNotificationTray::ShowMessageCenter() {
  message_center_delegate_ =
      new MessageCenterWidgetDelegate(this,
                                      message_center_tray_.get(),
                                      false,  // settings initally invisible
                                      GetPositionInfo());

  return true;
}

void WebNotificationTray::HideMessageCenter() {
  if (message_center_delegate_) {
    views::Widget* widget = message_center_delegate_->GetWidget();
    if (widget)
      widget->Close();
  }
}

bool WebNotificationTray::ShowNotifierSettings() {
  if (message_center_delegate_) {
    message_center_delegate_->SetSettingsVisible(true);
    return true;
  }
  message_center_delegate_ =
      new MessageCenterWidgetDelegate(this,
                                      message_center_tray_.get(),
                                      true,  // settings initally visible
                                      GetPositionInfo());

  return true;
}

void WebNotificationTray::OnMessageCenterTrayChanged() {
  if (status_icon_) {
    bool quiet_mode_state = message_center()->IsQuietMode();
    if (last_quiet_mode_state_ != quiet_mode_state) {
      last_quiet_mode_state_ = quiet_mode_state;

      // Quiet mode has changed, update the quiet mode menu.
      status_icon_menu_->SetCommandIdChecked(kToggleQuietMode,
                                             quiet_mode_state);
    }
  }

  // See the comments in ash/system/web_notification/web_notification_tray.cc
  // for why PostTask.
  should_update_tray_content_ = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&WebNotificationTray::UpdateStatusIcon, AsWeakPtr()));
}

void WebNotificationTray::OnStatusIconClicked() {
  // TODO(dewittj): It's possible GetNativeScreen is wrong for win-aura.
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  mouse_click_point_ = screen->GetCursorScreenPoint();
  message_center_tray_->ToggleMessageCenterBubble();
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

void WebNotificationTray::UpdateStatusIcon() {
  if (!should_update_tray_content_)
    return;
  should_update_tray_content_ = false;

  int unread_notifications = message_center()->UnreadNotificationCount();

  string16 tool_tip;
  if (unread_notifications > 0) {
    string16 str_unread_count = base::FormatNumber(unread_notifications);
    tool_tip = l10n_util::GetStringFUTF16(IDS_MESSAGE_CENTER_TOOLTIP_UNREAD,
                                          str_unread_count);
  } else {
    tool_tip = l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_TOOLTIP);
  }

  gfx::ImageSkia* icon_image = GetIcon(
      unread_notifications,
      message_center()->IsQuietMode());

  if (status_icon_) {
    status_icon_->SetImage(*icon_image);
    status_icon_->SetToolTip(tool_tip);
    return;
  }

  CreateStatusIcon(*icon_image, tool_tip);
}

void WebNotificationTray::SendHideMessageCenter() {
  message_center_tray_->HideMessageCenterBubble();
}

void WebNotificationTray::MarkMessageCenterHidden() {
  if (message_center_delegate_) {
    message_center_tray_->MarkMessageCenterHidden();
    message_center_delegate_ = NULL;
  }
}

PositionInfo WebNotificationTray::GetPositionInfo() {
  PositionInfo pos_info;

  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  gfx::Rect work_area = screen->GetPrimaryDisplay().work_area();
  work_area.Inset(kScreenEdgePadding, kScreenEdgePadding);

  gfx::Point corner = internal::GetClosestCorner(work_area, mouse_click_point_);

  pos_info.taskbar_alignment = internal::GetTaskbarAlignment();

  // We assume the taskbar is either at the top or at the bottom if we are not
  // able to find it.
  if (pos_info.taskbar_alignment == ALIGNMENT_NONE) {
    if (mouse_click_point_.y() > corner.y())
      pos_info.taskbar_alignment = ALIGNMENT_TOP;
    else
      pos_info.taskbar_alignment = ALIGNMENT_BOTTOM;
  }

  pos_info.message_center_alignment =
      internal::GetAnchorAlignment(work_area, corner);

  pos_info.inital_anchor_point = corner;
  pos_info.max_height = work_area.height();

  if (work_area.Contains(mouse_click_point_)) {
    // Message center is in the work area. So position it few pixels above the
    // mouse click point if alignemnt is towards bottom and few pixels below if
    // alignment is towards top.
    pos_info.inital_anchor_point.set_y(
        mouse_click_point_.y() +
        (pos_info.message_center_alignment & ALIGNMENT_BOTTOM ? -kMouseOffset
                                                              : kMouseOffset));

    // Subtract the distance between mouse click point and the closest
    // (insetted) edge from the max height to show the message center within the
    // (insetted) work area bounds. Also subtract the offset from the mouse
    // click point we added earlier.
    pos_info.max_height -=
        std::abs(mouse_click_point_.y() - corner.y()) + kMouseOffset;
  }
  return pos_info;
}

MessageCenterTray* WebNotificationTray::GetMessageCenterTray() {
  return message_center_tray_.get();
}

void WebNotificationTray::CreateStatusIcon(const gfx::ImageSkia& image,
                                           const string16& tool_tip) {
  if (status_icon_)
    return;

  StatusTray* status_tray = g_browser_process->status_tray();
  if (!status_tray)
    return;

  status_icon_ = status_tray->CreateStatusIcon(
      StatusTray::NOTIFICATION_TRAY_ICON, image, tool_tip);
  if (!status_icon_)
    return;

  status_icon_->AddObserver(this);
  AddQuietModeMenu(status_icon_);
}

void WebNotificationTray::DestroyStatusIcon() {
  if (!status_icon_)
    return;

  status_icon_->RemoveObserver(this);
  StatusTray* status_tray = g_browser_process->status_tray();
  if (status_tray)
    status_tray->RemoveStatusIcon(status_icon_);
  status_icon_menu_ = NULL;
  status_icon_ = NULL;
}

void WebNotificationTray::AddQuietModeMenu(StatusIcon* status_icon) {
  DCHECK(status_icon);

  scoped_ptr<StatusIconMenuModel> menu(new StatusIconMenuModel(this));
  menu->AddCheckItem(kToggleQuietMode,
                     l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_QUIET_MODE));
  menu->SetCommandIdChecked(kToggleQuietMode, message_center()->IsQuietMode());
  menu->AddItem(kEnableQuietModeHour,
                l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_QUIET_MODE_1HOUR));
  menu->AddItem(kEnableQuietModeDay,
                l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_QUIET_MODE_1DAY));

  status_icon_menu_ = menu.get();
  status_icon->SetContextMenu(menu.Pass());
}

MessageCenterWidgetDelegate*
WebNotificationTray::GetMessageCenterWidgetDelegateForTest() {
  return message_center_delegate_;
}

}  // namespace message_center
