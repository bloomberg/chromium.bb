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

// TODO(nkostylev): Switch to GridLayout.

// Y offset for the 'installing updates' label.
const int kManualRebootLabelY = 100;
// Y offset for the 'installing updates' label.
const int kInstallingUpdatesLabelY = 130;
// Y offset for the progress bar.
const int kProgressBarY = 200;
// Y offset for the 'computer will be rebooted' label.
const int kRebootLabelY = 250;
// Y offset for the 'ESCAPE to skip' label.
const int kEscapeToSkipLabelY = 300;
// Progress bar width.
const int kProgressBarWidth = 450;
// Horizontal spacing (ex. min left and right margins for label on the screen).
const int kHorizontalSpacing = 25;

// Label color.
const SkColor kLabelColor = 0xFF000000;
const SkColor kSkipLabelColor = 0xFFAA0000;
const SkColor kManualRebootLabelColor = 0xFFAA0000;

}  // namespace

namespace chromeos {

UpdateView::UpdateView(chromeos::ScreenObserver* observer)
    : escape_accelerator_(base::VKEY_ESCAPE, false, false, false),
      installing_updates_label_(NULL),
      reboot_label_(NULL),
      manual_reboot_label_(NULL),
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
  gfx::Font label_font = res_bundle.GetFont(ResourceBundle::MediumFont);

  InitLabel(&installing_updates_label_);
  InitLabel(&reboot_label_);
  InitLabel(&manual_reboot_label_);
  installing_updates_label_->SetFont(label_font);
  reboot_label_->SetFont(label_font);
  manual_reboot_label_->SetFont(label_font);
  manual_reboot_label_->SetVisible(false);
  manual_reboot_label_->SetColor(kManualRebootLabelColor);

  progress_bar_ = new views::ProgressBar();
  AddChildView(progress_bar_);

  UpdateLocalizedStrings();

#if !defined(OFFICIAL_BUILD)
  escape_to_skip_label_ = new views::Label();
  escape_to_skip_label_->SetColor(kSkipLabelColor);
  escape_to_skip_label_->SetFont(label_font);
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
  reboot_label_->SetText(l10n_util::GetString(IDS_INSTALLING_UPDATE_DESC));
  manual_reboot_label_->SetText(l10n_util::GetString(IDS_UPDATE_COMPLETED));
}

void UpdateView::AddProgress(int ticks) {
  progress_bar_->AddProgress(ticks);
}

void UpdateView::SetProgress(int progress) {
  progress_bar_->SetProgress(progress);
}

void UpdateView::ShowManualRebootInfo() {
  installing_updates_label_->SetVisible(false);
  reboot_label_->SetVisible(false);
  manual_reboot_label_->SetVisible(true);
}

// Sets the bounds of the view, placing it at the center of the screen
// with the |y| coordinate provided. |width| could specify desired view width
// otherwise preferred width/height are used.
// |x_center| specifies screen center.
static void setViewBounds(
    views::View* view, int x_center, int y, int width = -1) {
  int preferred_width = (width >= 0) ? width : view->GetPreferredSize().width();
  int preferred_height = view->GetPreferredSize().height();
  view->SetBounds(
      x_center - preferred_width / 2,
      y,
      preferred_width,
      preferred_height);
}

void UpdateView::Layout() {
  int x_center = width() / 2;
  int max_width = width() - GetInsets().width() - 2 * kHorizontalSpacing;
  installing_updates_label_->SizeToFit(max_width);
  reboot_label_->SizeToFit(max_width);
  manual_reboot_label_->SizeToFit(max_width);

  setViewBounds(manual_reboot_label_, x_center, kManualRebootLabelY);
  setViewBounds(installing_updates_label_, x_center, kInstallingUpdatesLabelY);
  setViewBounds(progress_bar_, x_center, kProgressBarY, kProgressBarWidth);
  setViewBounds(reboot_label_, x_center, kRebootLabelY);
#if !defined(OFFICIAL_BUILD)
  setViewBounds(escape_to_skip_label_, x_center, kEscapeToSkipLabelY);
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

void UpdateView::InitLabel(views::Label** label) {
  *label = new views::Label();
  (*label)->SetColor(kLabelColor);
  (*label)->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
  (*label)->SetMultiLine(true);
  AddChildView(*label);
}

}  // namespace chromeos
