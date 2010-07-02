// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/update_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/update_view.h"

namespace {

// Update window should appear for at least kMinimalUpdateTime seconds.
const int kMinimalUpdateTime = 3;

// Progress bar increment step.
const int kBeforeUpdateCheckProgressIncrement = 10;
const int kAfterUpdateCheckProgressIncrement = 10;
const int kUpdateCompleteProgressIncrement = 75;

}  // anonymous namespace

namespace chromeos {

UpdateScreen::UpdateScreen(WizardScreenDelegate* delegate)
    : DefaultViewScreen<chromeos::UpdateView>(delegate),
      update_result_(UPGRADE_STARTED),
      update_error_(GOOGLE_UPDATE_NO_ERROR),
      checking_for_update_(true) {
}

UpdateScreen::~UpdateScreen() {
  // Remove pointer to this object from view.
  if (view())
    view()->set_controller(NULL);
  // Google Updater is holding a pointer to us until it reports status,
  // so we need to remove it in case we were still listening.
  if (google_updater_.get())
    google_updater_->set_status_listener(NULL);
}

void UpdateScreen::OnReportResults(GoogleUpdateUpgradeResult result,
                                   GoogleUpdateErrorCode error_code,
                                   const std::wstring& version) {
  // Drop the last reference to the object so that it gets cleaned up here.
  if (google_updater_.get()) {
    google_updater_->set_status_listener(NULL);
    google_updater_ = NULL;
  }
  // Depending on the result decide what to do next.
  update_result_ = result;
  update_error_ = error_code;
  LOG(INFO) << "Update result: " << result;
  if (error_code != GOOGLE_UPDATE_NO_ERROR)
    LOG(INFO) << "Update error code: " << error_code;
  LOG(INFO) << "Update version: " << version;
  switch (update_result_) {
    case UPGRADE_IS_AVAILABLE:
      checking_for_update_ = false;
      // Advance view progress bar.
      view()->AddProgress(kAfterUpdateCheckProgressIncrement);
      // Create new Google Updater instance and install the update.
      google_updater_ = CreateGoogleUpdate();
      google_updater_->CheckForUpdate(true, NULL);
      LOG(INFO) << "Installing an update";
      break;
    case UPGRADE_SUCCESSFUL:
      view()->AddProgress(kUpdateCompleteProgressIncrement);
      minimal_update_time_timer_.Stop();
      checking_for_update_ = false;
      // TODO(nkostylev): Call reboot API. http://crosbug.com/4002
      ExitUpdate();
      break;
    case UPGRADE_ALREADY_UP_TO_DATE:
      checking_for_update_ = false;
      view()->AddProgress(kAfterUpdateCheckProgressIncrement);
      // Fall through.
    case UPGRADE_ERROR:
      if (MinimalUpdateTimeElapsed()) {
        ExitUpdate();
      }
      break;
    default:
      NOTREACHED();
  }
}

void UpdateScreen::StartUpdate() {
  // Reset view.
  view()->Reset();
  view()->set_controller(this);

  // Start the minimal update time timer.
  minimal_update_time_timer_.Start(
      base::TimeDelta::FromSeconds(kMinimalUpdateTime),
      this,
      &UpdateScreen::OnMinimalUpdateTimeElapsed);

  // Create Google Updater object and check if there is an update available.
  checking_for_update_ = true;
  google_updater_ = CreateGoogleUpdate();
  google_updater_->CheckForUpdate(false, NULL);
  view()->AddProgress(kBeforeUpdateCheckProgressIncrement);
  LOG(INFO) << "Checking for update";
}

void UpdateScreen::CancelUpdate() {
#if !defined(OFFICIAL_BUILD)
  update_result_ = UPGRADE_ALREADY_UP_TO_DATE;
  update_error_ = GOOGLE_UPDATE_NO_ERROR;
  ExitUpdate();
#endif
}

void UpdateScreen::ExitUpdate() {
  if (google_updater_.get()) {
    google_updater_->set_status_listener(NULL);
    google_updater_ = NULL;
  }
  minimal_update_time_timer_.Stop();
  ScreenObserver* observer = delegate()->GetObserver(this);
  if (observer) {
    switch (update_result_) {
      case UPGRADE_ALREADY_UP_TO_DATE:
        observer->OnExit(ScreenObserver::UPDATE_NOUPDATE);
        break;
      case UPGRADE_SUCCESSFUL:
        observer->OnExit(ScreenObserver::UPDATE_INSTALLED);
        break;
      case UPGRADE_ERROR:
        if (checking_for_update_) {
          observer->OnExit(ScreenObserver::UPDATE_ERROR_CHECKING_FOR_UPDATE);
        } else {
          observer->OnExit(ScreenObserver::UPDATE_ERROR_UPDATING);
        }
        break;
      default:
        NOTREACHED();
    }
  }
}

bool UpdateScreen::MinimalUpdateTimeElapsed() {
  return !minimal_update_time_timer_.IsRunning();
}

GoogleUpdate* UpdateScreen::CreateGoogleUpdate() {
  GoogleUpdate* updater = new GoogleUpdate();
  updater->set_status_listener(this);
  return updater;
}

void UpdateScreen::OnMinimalUpdateTimeElapsed() {
  if (update_result_ == UPGRADE_SUCCESSFUL ||
      update_result_ == UPGRADE_ALREADY_UP_TO_DATE ||
      update_result_ == UPGRADE_ERROR) {
    ExitUpdate();
  }
}

}  // namespace chromeos
