// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/ime_menu/ime_menu_tray.h"

#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

class ImeMenuLabel : public views::Label {
 public:
  ImeMenuLabel() {}
  ~ImeMenuLabel() override {}

  // views:Label:
  gfx::Size GetPreferredSize() const override {
    return gfx::Size(kTrayImeIconSize, kTrayImeIconSize);
  }
  int GetHeightForWidth(int width) const override { return kTrayImeIconSize; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ImeMenuLabel);
};

}  // namespace

ImeMenuTray::ImeMenuTray(WmShelf* wm_shelf)
    : TrayBackgroundView(wm_shelf), label_(new ImeMenuLabel()) {
  SetupLabelForTray(label_);
  tray_container()->AddChildView(label_);
  SetContentsBackground();
  WmShell::Get()->system_tray_notifier()->AddIMEObserver(this);
}

ImeMenuTray::~ImeMenuTray() {
  WmShell::Get()->system_tray_notifier()->RemoveIMEObserver(this);
}

void ImeMenuTray::SetShelfAlignment(ShelfAlignment alignment) {
  TrayBackgroundView::SetShelfAlignment(alignment);
  tray_container()->SetBorder(views::Border::NullBorder());
}

base::string16 ImeMenuTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_IME_MENU_ACCESSIBLE_NAME);
}

void ImeMenuTray::HideBubbleWithView(const views::TrayBubbleView* bubble_view) {
}

void ImeMenuTray::ClickedOutsideBubble() {}

bool ImeMenuTray::PerformAction(const ui::Event& event) {
  SetTrayActivation(!draw_background_as_active());
  return true;
}

void ImeMenuTray::OnIMERefresh() {
  UpdateTrayLabel();
}

void ImeMenuTray::OnIMEMenuActivationChanged(bool is_activated) {
  SetVisible(is_activated);
  if (is_activated)
    UpdateTrayLabel();
  else
    SetTrayActivation(false);
}

void ImeMenuTray::UpdateTrayLabel() {
  WmShell::Get()->system_tray_delegate()->GetCurrentIME(&current_ime_);

  // Updates the tray label based on the current input method.
  if (current_ime_.third_party)
    label_->SetText(current_ime_.short_name + base::UTF8ToUTF16("*"));
  else
    label_->SetText(current_ime_.short_name);
}

void ImeMenuTray::SetTrayActivation(bool is_active) {
  SetDrawBackgroundAsActive(is_active);
  // TODO(azurewei): Shows/hides the IME menu tray bubble based on |is_active|.
}

}  // namespace ash
