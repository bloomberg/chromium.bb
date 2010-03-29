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

// Update window should appear for at least kMinimalUpdateTime seconds.
const int kMinimalUpdateTime = 3;

// Progress bar increment step.
const int kUpdateCheckProgressIncrement = 20;
const int kUpdateCompleteProgressIncrement = 75;

}  // namespace

namespace chromeos {

UpdateView::UpdateView(chromeos::ScreenObserver* observer)
    : escape_accelerator_(base::VKEY_ESCAPE, false, false, false),
      installing_updates_label_(NULL),
      progress_bar_(NULL),
      observer_(observer),
      update_result_(UPGRADE_STARTED),
      update_error_(GOOGLE_UPDATE_NO_ERROR) {
}

UpdateView::~UpdateView() {
  // Google Updater is holding a pointer to us until it reports status,
  // so we need to remove it in case we were stll listening.
  if (google_updater_.get())
    google_updater_->set_status_listener(NULL);
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
  if (a.GetKeyCode() == base::VKEY_ESCAPE) {
    // ESCAPE cancels Chrome OS update on first startup.
    update_result_ = UPGRADE_ALREADY_UP_TO_DATE;
    update_error_ = GOOGLE_UPDATE_NO_ERROR;
    minimal_update_time_timer_.Stop();
    ExitUpdate();
    return true;
  }
#endif
  return false;
}

void UpdateView::StartUpdate() {
  // Zero progress.
  progress_bar_->SetProgress(0);
  // Start the minimal update time timer.
  DCHECK(!minimal_update_time_timer_.IsRunning());
  minimal_update_time_timer_.Start(
      base::TimeDelta::FromSeconds(kMinimalUpdateTime),
      this,
      &UpdateView::OnMinimalUpdateTimeElapsed);
  // Create Google Updater object and check if there is an update available.
  google_updater_ = new GoogleUpdate();
  google_updater_->set_status_listener(this);
  google_updater_->CheckForUpdate(false, NULL);
  // Add keyboard accelerator for canceling the update.
  AddAccelerator(escape_accelerator_);
}

void UpdateView::ExitUpdate() {
  progress_bar_->SetProgress(100);
  if (observer_) {
    switch (update_result_) {
      case UPGRADE_ALREADY_UP_TO_DATE:
        observer_->OnExit(ScreenObserver::UPDATE_NOUPDATE);
        break;
      case UPGRADE_SUCCESSFUL:
        observer_->OnExit(ScreenObserver::UPDATE_INSTALLED);
        break;
      case UPGRADE_ERROR:
        if (update_error_ == GOOGLE_UPDATE_ERROR_UPDATING) {
          observer_->OnExit(ScreenObserver::UPDATE_NETWORK_ERROR);
        } else {
          // TODO(denisromanov): figure out better what to do if
          // some other error has occurred.
          observer_->OnExit(ScreenObserver::UPDATE_OTHER_ERROR);
        }
        break;
      default:
        NOTREACHED();
    }
  }
  RemoveAccelerator(escape_accelerator_);
}

void UpdateView::OnMinimalUpdateTimeElapsed() {
  if (update_result_ == UPGRADE_SUCCESSFUL ||
      update_result_ == UPGRADE_ALREADY_UP_TO_DATE ||
      update_result_ == UPGRADE_ERROR) {
    ExitUpdate();
  }
}

void UpdateView::OnReportResults(GoogleUpdateUpgradeResult result,
                                 GoogleUpdateErrorCode error_code,
                                 const std::wstring& version) {
  // Drop the last reference to the object so that it gets cleaned up here.
  google_updater_ = NULL;
  // Depending on the result decide what to do next.
  update_result_ = result;
  update_error_ = error_code;
  switch (update_result_) {
    case UPGRADE_IS_AVAILABLE:
      // Advance progress bar.
      progress_bar_->AddProgress(kUpdateCheckProgressIncrement);
      // Create new Google Updater instance and install the update.
      google_updater_ = new GoogleUpdate();
      google_updater_->set_status_listener(this);
      google_updater_->CheckForUpdate(true, NULL);
      break;
    case UPGRADE_SUCCESSFUL:
      // Advance progress bar.
      progress_bar_->AddProgress(kUpdateCompleteProgressIncrement);
      // Fall through.
    case UPGRADE_ALREADY_UP_TO_DATE:
    case UPGRADE_ERROR:
      if (minimal_update_time_timer_.IsRunning()) {
        // Minimal time required for the update screen to remain visible
        // has not yet passed.
        progress_bar_->AddProgress(kUpdateCheckProgressIncrement);
      } else {
        ExitUpdate();
      }
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace chromeos
