// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/message_center/message_center_widget_delegate.h"

#include <complex>

#include "chrome/browser/ui/views/message_center/message_center_frame_view.h"
#include "content/public/browser/user_metrics.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/gfx/screen.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_util.h"
#include "ui/message_center/views/message_center_view.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

namespace message_center {

MessageCenterWidgetDelegate::MessageCenterWidgetDelegate(
    WebNotificationTray* tray,
    MessageCenterTray* mc_tray,
    bool initially_settings_visible,
    const PositionInfo& pos_info)
    : MessageCenterView(tray->message_center(),
                        mc_tray,
                        pos_info.max_height,
                        initially_settings_visible,
                        pos_info.message_center_alignment &
                            ALIGNMENT_TOP),  // Show buttons on top if message
                                             // center is top aligned
      pos_info_(pos_info),
      tray_(tray) {
  // A WidgetDelegate should be deleted on DeleteDelegate.
  set_owned_by_client();

  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
  layout->set_spread_blank_space(true);
  SetLayoutManager(layout);

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  if (get_use_acceleration_when_possible()) {
    SetPaintToLayer(true);
    SetFillsBoundsOpaquely(true);
  }

  InitWidget();
}

MessageCenterWidgetDelegate::~MessageCenterWidgetDelegate() {
  views::Widget* widget = GetWidget();
  if (widget) {
    widget->RemoveObserver(this);
  }
}

views::View* MessageCenterWidgetDelegate::GetContentsView() {
  return this;
}

views::NonClientFrameView*
MessageCenterWidgetDelegate::CreateNonClientFrameView(views::Widget* widget) {
  MessageCenterFrameView* frame_view = new MessageCenterFrameView();
  border_insets_ = frame_view->GetInsets();
  return frame_view;
}

void MessageCenterWidgetDelegate::DeleteDelegate() {
  delete this;
}

views::Widget* MessageCenterWidgetDelegate::GetWidget() {
  return View::GetWidget();
}

const views::Widget* MessageCenterWidgetDelegate::GetWidget() const {
  return View::GetWidget();
}

void MessageCenterWidgetDelegate::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  if (!active) {
    tray_->SendHideMessageCenter();
  }
}

void MessageCenterWidgetDelegate::OnWidgetClosing(views::Widget* widget) {
  SetIsClosing(true);
  tray_->MarkMessageCenterHidden();
}

void MessageCenterWidgetDelegate::PreferredSizeChanged() {
  GetWidget()->SetBounds(GetMessageCenterBounds());
  views::View::PreferredSizeChanged();
}

gfx::Size MessageCenterWidgetDelegate::GetPreferredSize() {
  int preferred_width = kNotificationWidth + 2 * kMarginBetweenItems;
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

gfx::Size MessageCenterWidgetDelegate::GetMaximumSize() {
  gfx::Size size = GetPreferredSize();
  return size;
}

int MessageCenterWidgetDelegate::GetHeightForWidth(int width) {
  int height = MessageCenterView::GetHeightForWidth(width);
  return (pos_info_.max_height != 0) ?
    std::min(height, pos_info_.max_height - border_insets_.height()) : height;
}

bool MessageCenterWidgetDelegate::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() != ui::VKEY_ESCAPE)
    return false;
  tray_->SendHideMessageCenter();
  return true;
}

void MessageCenterWidgetDelegate::InitWidget() {
  views::Widget* widget = new views::Widget();
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_BUBBLE);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.delegate = this;
  params.keep_on_top = true;
  params.top_level = true;
  widget->Init(params);

#if defined(OS_WIN)
  // Remove the Message Center from taskbar and alt-tab rotation.
  HWND hwnd = views::HWNDForWidget(widget);
  LONG_PTR ex_styles = ::GetWindowLongPtr(hwnd, GWL_EXSTYLE);
  ex_styles |= WS_EX_TOOLWINDOW;
  ::SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex_styles);
#endif

  widget->AddObserver(this);
  widget->StackAtTop();
  widget->SetAlwaysOnTop(true);

  const NotificationList::Notifications& notifications =
      tray_->message_center()->GetVisibleNotifications();
  SetNotifications(notifications);

  widget->SetBounds(GetMessageCenterBounds());
  widget->Show();
  widget->Activate();
}

gfx::Point MessageCenterWidgetDelegate::GetCorrectedAnchor(
    gfx::Size calculated_size) {
  gfx::Point corrected_anchor = pos_info_.inital_anchor_point;

  // Inset the width slightly so that the click point is not exactly on the edge
  // of the message center but somewhere within the middle 60 %.
  int insetted_width = (calculated_size.width() * 4) / 5;

  if (pos_info_.taskbar_alignment == ALIGNMENT_TOP ||
      pos_info_.taskbar_alignment == ALIGNMENT_BOTTOM) {
    int click_point_x = tray_->mouse_click_point().x();

    if (pos_info_.message_center_alignment & ALIGNMENT_RIGHT) {
      int opposite_x_corner =
          pos_info_.inital_anchor_point.x() - insetted_width;

      // If the click point is outside the x axis length of the message center,
      // push the message center towards the left to align with the click point.
      if (opposite_x_corner > click_point_x)
        corrected_anchor.set_x(pos_info_.inital_anchor_point.x() -
                               (opposite_x_corner - click_point_x));
    } else {
      int opposite_x_corner =
          pos_info_.inital_anchor_point.x() + insetted_width;

      if (opposite_x_corner < click_point_x)
        corrected_anchor.set_x(pos_info_.inital_anchor_point.x() +
                               (click_point_x - opposite_x_corner));
    }
  } else if (pos_info_.taskbar_alignment == ALIGNMENT_LEFT ||
             pos_info_.taskbar_alignment == ALIGNMENT_RIGHT) {
    int click_point_y = tray_->mouse_click_point().y();

    if (pos_info_.message_center_alignment & ALIGNMENT_BOTTOM) {
      int opposite_y_corner =
          pos_info_.inital_anchor_point.y() - insetted_width;

      // If the click point is outside the y axis length of the message center,
      // push the message center upwards to align with the click point.
      if (opposite_y_corner > click_point_y)
        corrected_anchor.set_y(pos_info_.inital_anchor_point.y() -
                               (opposite_y_corner - click_point_y));
    } else {
      int opposite_y_corner =
          pos_info_.inital_anchor_point.y() + insetted_width;

      if (opposite_y_corner < click_point_y)
        corrected_anchor.set_y(pos_info_.inital_anchor_point.y() +
                               (click_point_y - opposite_y_corner));
    }
  }
  return corrected_anchor;
}

gfx::Rect MessageCenterWidgetDelegate::GetMessageCenterBounds() {
  gfx::Size size = GetPreferredSize();

  // Make space for borders on sides.
  size.Enlarge(border_insets_.width(), border_insets_.height());
  gfx::Rect bounds(size);

  gfx::Point corrected_anchor = GetCorrectedAnchor(size);

  if (pos_info_.message_center_alignment & ALIGNMENT_TOP)
    bounds.set_y(corrected_anchor.y());
  if (pos_info_.message_center_alignment & ALIGNMENT_BOTTOM)
    bounds.set_y(corrected_anchor.y() - size.height());
  if (pos_info_.message_center_alignment & ALIGNMENT_LEFT)
    bounds.set_x(corrected_anchor.x());
  if (pos_info_.message_center_alignment & ALIGNMENT_RIGHT)
    bounds.set_x(corrected_anchor.x() - size.width());

  return bounds;
}

}  // namespace message_center
