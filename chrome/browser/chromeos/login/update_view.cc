// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/update_view.h"

#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/update_screen.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "views/border.h"
#include "views/controls/label.h"
#include "views/controls/progress_bar.h"
#include "views/focus/focus_manager.h"
#include "views/widget/widget.h"

using views::Background;
using views::Label;
using views::View;
using views::Widget;

namespace {

// Y offset for the 'installing updates' label.
const int kInstallingUpdatesLabelY = 200;
// Y offset for the progress bar.
const int kProgressBarY = 250;
// Y offset for the 'ESCAPE to skip' label.
const int kEscapeToSkipLabelY = 290;
// Progress bar width.
const int kProgressBarWidth = 450;

// Label color.
const SkColor kLabelColor = 0xFF000000;
const SkColor kSkipLabelColor = 0xFFAA0000;

}  // namespace

namespace chromeos {

UpdateView::UpdateView(chromeos::ScreenObserver* observer)
    : escape_accelerator_(base::VKEY_ESCAPE, false, false, false),
      installing_updates_label_(NULL),
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
  installing_updates_label_->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
  installing_updates_label_->SetMultiLine(true);

  progress_bar_ = new views::ProgressBar();

  UpdateLocalizedStrings();
  AddChildView(installing_updates_label_);
  AddChildView(progress_bar_);

#if !defined(OFFICIAL_BUILD)
  escape_to_skip_label_ = new views::Label();
  escape_to_skip_label_->SetColor(kSkipLabelColor);
  escape_to_skip_label_->SetFont(base_font);
  escape_to_skip_label_->SetText(
      L"Press ESCAPE to skip (Non-official builds only)");
  AddChildView(escape_to_skip_label_);
#endif
}

void UpdateView::Reset() {
  progress_bar_->SetProgress(0);
#if !defined(OFFICIAL_BUILD)
  AddAccelerator(escape_accelerator_);
#endif
}

void UpdateView::UpdateLocalizedStrings() {
  installing_updates_label_->SetText(
      l10n_util::GetStringF(IDS_INSTALLING_UPDATE,
                            l10n_util::GetString(IDS_PRODUCT_OS_NAME)));
}

void UpdateView::AddProgress(int ticks) {
  progress_bar_->AddProgress(ticks);
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
#if !defined(OFFICIAL_BUILD)
  preferred_width = escape_to_skip_label_->GetPreferredSize().width();
  preferred_height = escape_to_skip_label_->GetPreferredSize().height();
  escape_to_skip_label_->SetBounds(
      x_center - preferred_width / 2,
      kEscapeToSkipLabelY,
      preferred_width,
      preferred_height);
#endif
  SchedulePaint();
}

bool UpdateView::AcceleratorPressed(const views::Accelerator& a) {
#if !defined(OFFICIAL_BUILD)
  RemoveAccelerator(escape_accelerator_);
  if (a.GetKeyCode() == base::VKEY_ESCAPE) {
    if (controller_ != NULL) {
      controller_->CancelUpdate();
    }
    return true;
  }
#endif
  return false;
}

}  // namespace chromeos
