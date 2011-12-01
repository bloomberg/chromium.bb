// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/shutdown_button.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"
#include "ui/views/background.h"

namespace {

// Style parameters for Shutdown button.

// Top/Left padding to locale the shutdown button.
const int kTopPadding = 0;
const int kLeftPadding = 0;

// Normal/Hover colors.
const SkColor kShutdownColor = 0xFEFEFE;
const SkColor kShutdownTextColor = 0xFF606060;

// Padding inside button.
const int kVerticalPadding = 13;
const int kIconTextPadding = 10;
const int kHorizontalPadding = 13;

// Rounded corner radious.
const int kCornerRadius = 4;

class HoverBackground : public views::Background {
 public:
  HoverBackground(views::Background* normal, views::Background* hover)
      : normal_(normal), hover_(hover) {
  }

  // views::Background implementation.
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    views::TextButton* button = static_cast<views::TextButton*>(view);
    if (button->state() == views::CustomButton::BS_HOT) {
      hover_->Paint(canvas, view);
    } else {
      normal_->Paint(canvas, view);
    }
  }

 private:
  views::Background* normal_;
  views::Background* hover_;

  DISALLOW_COPY_AND_ASSIGN(HoverBackground);
};

}  // namespace

namespace chromeos {

ShutdownButton::ShutdownButton()
    : ALLOW_THIS_IN_INITIALIZER_LIST(TextButton(this, string16())) {
}

void ShutdownButton::Init() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SetIcon(*rb.GetBitmapNamed(IDR_ICON_POWER20));
  set_icon_text_spacing(kIconTextPadding);
  set_focusable(true);
  set_id(VIEW_ID_SCREEN_LOCKER_SHUTDOWN);
  // Set label colors.
  SetEnabledColor(kShutdownTextColor);
  SetDisabledColor(kShutdownTextColor);
  SetHighlightColor(kShutdownTextColor);
  SetHoverColor(kShutdownTextColor);
  static_cast<views::TextButtonBorder*>(border())->copy_normal_set_to_hot_set();
  set_animate_on_state_change(false);
  // Setup round shapes.
  set_background(
      new HoverBackground(
          CreateRoundedBackground(kCornerRadius, 0, kShutdownColor, 0),
          CreateRoundedBackground(kCornerRadius, 0, kShutdownColor, 0)));
  set_border(
      views::Border::CreateEmptyBorder(kVerticalPadding,
                                       kHorizontalPadding,
                                       kVerticalPadding,
                                       kHorizontalPadding));
  OnLocaleChanged();  // set text
}

void ShutdownButton::LayoutIn(views::View* parent) {
  // No RTL for now. RTL will be handled in new WebUI based Login/Locker.
  gfx::Size button_size = GetPreferredSize();
  SetBounds(
      kLeftPadding, kTopPadding, button_size.width(), button_size.height());
}

gfx::NativeCursor ShutdownButton::GetCursor(const views::MouseEvent& event) {
  return IsEnabled() ? gfx::GetCursor(GDK_HAND2) : NULL;
}

void ShutdownButton::OnLocaleChanged() {
  SetText(l10n_util::GetStringUTF16(IDS_SHUTDOWN_BUTTON));
  if (parent()) {
    parent()->Layout();
    parent()->SchedulePaint();
  }
}

void ShutdownButton::ButtonPressed(views::Button* sender,
                                   const views::Event& event) {
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestShutdown();
}

}  // namespace chromeos
