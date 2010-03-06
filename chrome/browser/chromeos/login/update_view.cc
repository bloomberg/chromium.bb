// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/update_view.h"

#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "views/border.h"
#include "views/controls/label.h"
#include "views/widget/widget.h"
#include "views/window/window.h"
#include "views/window/window_gtk.h"

using views::Background;
using views::Label;
using views::View;
using views::Widget;

namespace {

// Y offset for the 'installing updates' label.
const int kInstallingUpdatesLabelY = 200;
// Labels colour.
const SkColor kLabelColor = 0xFF000000;

}  // namespace

namespace chromeos {

UpdateView::UpdateView(chromeos::ScreenObserver* observer)
    : installing_updates_label_(NULL),
      observer_(observer) {
}

UpdateView::~UpdateView() {
}

void UpdateView::Init() {
  // Use rounded-rect background.
  views::Painter* painter = chromeos::CreateWizardPainter(
      &chromeos::BorderDefinition::kScreenBorder);
  set_background(views::Background::CreateBackgroundPainter(true, painter));

  // Set UI elements' apperance.
  ResourceBundle& res_bundle = ResourceBundle::GetSharedInstance();
  gfx::Font base_font = res_bundle.GetFont(ResourceBundle::BaseFont);
  installing_updates_label_ = new views::Label();
  installing_updates_label_->SetColor(kLabelColor);
  installing_updates_label_->SetFont(base_font);

  UpdateLocalizedStrings();
  AddChildView(installing_updates_label_);
}

void UpdateView::UpdateLocalizedStrings() {
  installing_updates_label_->SetText(
      l10n_util::GetStringF(IDS_INSTALLING_UPDATE,
                            l10n_util::GetString(IDS_PRODUCT_OS_NAME)));
}

////////////////////////////////////////////////////////////////////////////////
// views::View: implementation:

void UpdateView::Layout() {
  int x = (width() -
           installing_updates_label_->GetPreferredSize().width()) / 2;
  int y = kInstallingUpdatesLabelY;
  installing_updates_label_->SetBounds(
      x,
      y,
      installing_updates_label_->GetPreferredSize().width(),
      installing_updates_label_->GetPreferredSize().height());
  SchedulePaint();
}

}  // namespace chromeos
