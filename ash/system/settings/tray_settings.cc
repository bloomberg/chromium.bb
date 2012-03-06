// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/settings/tray_settings.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace {

class SettingsView : public views::View {
 public:
  SettingsView() {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
          0, 0, 3));

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    views::ImageView* icon = new views::ImageView;
    icon->SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_SETTINGS).
        ToSkBitmap());
    AddChildView(icon);

    label_ = new views::Label(bundle.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_HELP_AND_SETTINGS));
    AddChildView(label_);
  }

  virtual ~SettingsView() {}

  // Overridden from views::View.
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    ash::Shell::GetInstance()->tray_delegate()->ShowSettings();
    return true;
  }

  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE {
    gfx::Font font = label_->font();
    label_->SetFont(font.DeriveFont(0,
          font.GetStyle() | gfx::Font::UNDERLINED));
    SchedulePaint();
  }

  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE {
    gfx::Font font = label_->font();
    label_->SetFont(font.DeriveFont(0,
          font.GetStyle() & ~gfx::Font::UNDERLINED));
    SchedulePaint();
  }

 private:
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(SettingsView);
};

}  // namespace

namespace ash {
namespace internal {

TraySettings::TraySettings() {}

TraySettings::~TraySettings() {}

views::View* TraySettings::CreateTrayView(user::LoginStatus status) {
  return NULL;
}

views::View* TraySettings::CreateDefaultView(user::LoginStatus status) {
  if (status == user::LOGGED_IN_NONE)
    return NULL;

  views::View* container = new views::View;
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 5);
  layout->set_spread_blank_space(true);
  container->SetLayoutManager(layout);

  views::View* settings = new SettingsView;
  container->AddChildView(settings);
  return container;
}

views::View* TraySettings::CreateDetailedView(user::LoginStatus status) {
  NOTIMPLEMENTED();
  return NULL;
}

void TraySettings::DestroyTrayView() {
}

void TraySettings::DestroyDefaultView() {
}

void TraySettings::DestroyDetailedView() {
}

}  // namespace internal
}  // namespace ash
