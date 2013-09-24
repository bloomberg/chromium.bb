// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/avatar_menu_button.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/avatar_menu_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profile_chooser_view.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"

static inline int Round(double x) {
  return static_cast<int>(x + 0.5);
}

AvatarMenuButton::AvatarMenuButton(Browser* browser, bool disabled)
    : MenuButton(NULL, string16(), this, false),
      browser_(browser),
      disabled_(disabled),
      is_rectangle_(false),
      old_height_(0) {
  // In RTL mode, the avatar icon should be looking the opposite direction.
  EnableCanvasFlippingForRTLUI(true);
}

AvatarMenuButton::~AvatarMenuButton() {
}

void AvatarMenuButton::OnPaint(gfx::Canvas* canvas) {
  if (!icon_.get())
    return;

  if (old_height_ != height() || button_icon_.isNull()) {
    old_height_ = height();
    button_icon_ = *profiles::GetAvatarIconForTitleBar(
        *icon_, is_rectangle_, width(), height()).ToImageSkia();
  }

  // Scale the image to fit the width of the button.
  int dst_width = std::min(button_icon_.width(), width());
  // Truncate rather than rounding, so that for odd widths we put the extra
  // pixel on the left.
  int dst_x = (width() - dst_width) / 2;

  // Scale the height and maintain aspect ratio. This means that the
  // icon may not fit in the view. That's ok, we just vertically center it.
  float scale =
      static_cast<float>(dst_width) / static_cast<float>(button_icon_.width());
  // Round here so that we minimize the aspect ratio drift.
  int dst_height = Round(button_icon_.height() * scale);
  // Round rather than truncating, so that for odd heights we select an extra
  // pixel below the image center rather than above.  This is because the
  // incognito image has shadows at the top that make the apparent center below
  // the real center.
  int dst_y = Round((height() - dst_height) / 2.0);
  canvas->DrawImageInt(button_icon_, 0, 0, button_icon_.width(),
      button_icon_.height(), dst_x, dst_y, dst_width, dst_height, false);
}

bool AvatarMenuButton::HitTestRect(const gfx::Rect& rect) const {
  if (disabled_)
    return false;
  return views::MenuButton::HitTestRect(rect);
}

void AvatarMenuButton::SetAvatarIcon(const gfx::Image& icon,
                                     bool is_rectangle) {
  icon_.reset(new gfx::Image(icon));
  button_icon_ = gfx::ImageSkia();
  is_rectangle_ = is_rectangle;
  SchedulePaint();
}

// views::MenuButtonListener implementation
void AvatarMenuButton::OnMenuButtonClicked(views::View* source,
                                           const gfx::Point& point) {
  if (disabled_)
    return;

  ShowAvatarBubble();
}

void AvatarMenuButton::ShowAvatarBubble() {
  gfx::Point origin;
  views::View::ConvertPointToScreen(this, &origin);
  gfx::Rect bounds(origin, size());
  if (profiles::IsNewProfileManagementEnabled()) {
    ProfileChooserView::ShowBubble(
        this, views::BubbleBorder::TOP_LEFT,
        views::BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR, bounds, browser_);
  } else {
    AvatarMenuBubbleView::ShowBubble(
        this, views::BubbleBorder::TOP_LEFT,
        views::BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR, bounds, browser_);
  }

  ProfileMetrics::LogProfileOpenMethod(ProfileMetrics::ICON_AVATAR_BUBBLE);
}
