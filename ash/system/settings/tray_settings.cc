// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/settings/tray_settings.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
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
    views::ImageView* icon = new views::ImageView;
    icon->SetImage(ui::ResourceBundle::GetSharedInstance().
        GetImageNamed(IDR_AURA_UBER_TRAY_SETTINGS).ToSkBitmap());
    views::Label* label = new views::Label(ASCIIToUTF16("Settings"));

    AddChildView(icon);
    AddChildView(label);
  }

  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    ash::Shell::GetInstance()->tray_delegate()->ShowSettings();
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SettingsView);
};

class HelpView : public views::View {
 public:
  HelpView() {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
          0, 0, 3));
    views::Label* label = new views::Label(ASCIIToUTF16("Help"));
    AddChildView(label);
  }

  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    ash::Shell::GetInstance()->tray_delegate()->ShowHelp();
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HelpView);
};

}  // namespace

views::View* TraySettings::CreateTrayView() {
  return NULL;
}

views::View* TraySettings::CreateDefaultView() {
  views::View* container = new views::View;
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 5);
  layout->set_spread_blank_space(true);
  container->SetLayoutManager(layout);

  views::View* settings = new SettingsView;
  container->AddChildView(settings);
  views::View* help = new HelpView;
  container->AddChildView(help);
  return container;
}

views::View* TraySettings::CreateDetailedView() {
  NOTIMPLEMENTED();
  return NULL;
}

void TraySettings::DestroyTrayView() {
}

void TraySettings::DestroyDefaultView() {
}

void TraySettings::DestroyDetailedView() {
}
