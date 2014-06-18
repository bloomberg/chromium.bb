// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_panel.h"

#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"

AppInfoPanel::AppInfoPanel(Profile* profile, const extensions::Extension* app)
    : profile_(profile), app_(app) {
}

AppInfoPanel::~AppInfoPanel() {
}

views::Label* AppInfoPanel::CreateHeading(const base::string16& text) const {
  views::Label* label = new views::Label(text);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumFont));
  return label;
}

views::View* AppInfoPanel::CreateVerticalStack() const {
  views::View* vertically_stacked_view = new views::View();
  vertically_stacked_view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical,
                           0,
                           0,
                           views::kRelatedControlVerticalSpacing));
  return vertically_stacked_view;
}
