// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/desktop_linux_browser_frame_view_layout.h"

#include "base/command_line.h"
#include "chrome/browser/ui/views/nav_button_provider.h"
#include "chrome/common/chrome_switches.h"

DesktopLinuxBrowserFrameViewLayout::DesktopLinuxBrowserFrameViewLayout(
    views::NavButtonProvider* nav_button_provider)
    : nav_button_provider_(nav_button_provider) {}

int DesktopLinuxBrowserFrameViewLayout::CaptionButtonY(
    chrome::FrameButtonDisplayType button_id,
    bool restored) const {
  gfx::Insets insets = nav_button_provider_->GetNavButtonMargin(button_id);
  return insets.top();
}

int DesktopLinuxBrowserFrameViewLayout::TopAreaPadding() const {
  return nav_button_provider_->GetTopAreaSpacing().left();
}

int DesktopLinuxBrowserFrameViewLayout::GetWindowCaptionSpacing(
    views::FrameButton button_id,
    bool leading_spacing,
    bool is_leading_button) const {
  gfx::Insets insets =
      nav_button_provider_->GetNavButtonMargin(GetButtonDisplayType(button_id));
  if (!leading_spacing)
    return insets.right();
  int spacing = insets.left();
  if (!is_leading_button)
    spacing += nav_button_provider_->GetInterNavButtonSpacing();
  return spacing;
}

void DesktopLinuxBrowserFrameViewLayout::LayoutNewStyleAvatar(
    views::View* host) {
  if (!new_avatar_button_)
    return;

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNativeAvatarButton)) {
    OpaqueBrowserFrameViewLayout::LayoutNewStyleAvatar(host);
    return;
  }
  gfx::Size button_size;
  gfx::Insets button_spacing;
  nav_button_provider_->CalculateCaptionButtonLayout(
      new_avatar_button_->GetPreferredSize(), delegate_->GetTopAreaHeight(),
      &button_size, &button_spacing);
  int inter_button_spacing = nav_button_provider_->GetInterNavButtonSpacing();

  int total_width =
      button_size.width() + button_spacing.right() + inter_button_spacing;

  int button_x = host->width() - trailing_button_start_ - total_width;
  int button_y = button_spacing.top();

  minimum_size_for_buttons_ += total_width;
  trailing_button_start_ += total_width;

  new_avatar_button_->SetBounds(button_x, button_y, button_size.width(),
                                button_size.height());
}

bool DesktopLinuxBrowserFrameViewLayout::ShouldDrawImageMirrored(
    views::ImageButton* button,
    ButtonAlignment alignment) const {
  return false;
}
