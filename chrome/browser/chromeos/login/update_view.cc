// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/update_view.h"

#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "views/border.h"
#include "views/controls/label.h"
#include "views/controls/progress_bar.h"
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
// Y offset for the progress bar.
const int kProgressBarY = 250;
// Progress bar width.
const int kProgressBarWidth = 450;
// Labels colour.
const SkColor kLabelColor = 0xFF000000;

// Timer constants.
const int kProgressTimerInterval = 200;
const int kProgressIncrement = 0.09;
}  // namespace

namespace chromeos {

UpdateView::UpdateView(chromeos::ScreenObserver* observer)
    : installing_updates_label_(NULL),
      progress_bar_(NULL),
      observer_(observer) {
}

UpdateView::~UpdateView() {
}

void UpdateView::Init() {
  // Use rounded-rect background.
  views::Painter* painter = chromeos::CreateWizardPainter(
      &chromeos::BorderDefinition::kScreenBorder);
  set_background(views::Background::CreateBackgroundPainter(true, painter));

  ResourceBundle& res_bundle = ResourceBundle::GetSharedInstance();
  gfx::Font base_font = res_bundle.GetFont(ResourceBundle::BaseFont);

  installing_updates_label_ = new views::Label();
  installing_updates_label_->SetColor(kLabelColor);
  installing_updates_label_->SetFont(base_font);

  progress_bar_ = new views::ProgressBar();

  UpdateLocalizedStrings();
  AddChildView(installing_updates_label_);
  AddChildView(progress_bar_);

  timer_.Start(base::TimeDelta::FromMilliseconds(kProgressTimerInterval),
               this,
               &UpdateView::OnTimerElapsed);
}

void UpdateView::UpdateLocalizedStrings() {
  installing_updates_label_->SetText(
      l10n_util::GetStringF(IDS_INSTALLING_UPDATE,
                            l10n_util::GetString(IDS_PRODUCT_OS_NAME)));
}

void UpdateView::Layout() {
  int x_center = width() / 2;
  int preferred_width = installing_updates_label_->GetPreferredSize().width();
  int preferred_height = installing_updates_label_->GetPreferredSize().height();
  installing_updates_label_->SetBounds(
      x_center - preferred_width / 2,
      kInstallingUpdatesLabelY,
      preferred_width,
      preferred_height);
  preferred_width = kProgressBarWidth;
  preferred_height = progress_bar_->GetPreferredSize().height();
  progress_bar_->SetBounds(
      x_center - preferred_width / 2,
      kProgressBarY,
      preferred_width,
      preferred_height);
  SchedulePaint();
}

void UpdateView::OnTimerElapsed() {
  double progress = progress_bar_->GetProgress();
  if (progress == 1.) {
    timer_.Stop();
    if (observer_) {
      observer_->OnExit(ScreenObserver::UPDATE_NOUPDATE);
    }
  } else {
    progress += kProgressIncrement;
    progress_bar_->SetProgress(progress);
  }
}

}  // namespace chromeos
