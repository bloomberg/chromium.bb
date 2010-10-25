// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/update_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/update_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace {

// Progress bar stages. Each represents progress bar value
// at the beginning of each stage.
// TODO(nkostylev): Base stage progress values on approximate time.
// TODO(nkostylev): Animate progress during each state.
const int kBeforeUpdateCheckProgress = 7;
const int kBeforeDownloadProgress = 14;
const int kBeforeVerifyingProgress = 74;
const int kBeforeFinalizingProgress = 81;
const int kProgressComplete = 100;

// Defines what part of update progress does download part takes.
const int kDownloadProgressIncrement = 60;

// Considering 10px shadow from each side.
const int kUpdateScreenWidth = 580;
const int kUpdateScreenHeight = 305;

}  // anonymous namespace

namespace chromeos {

UpdateScreen::UpdateScreen(WizardScreenDelegate* delegate)
    : DefaultViewScreen<chromeos::UpdateView>(delegate,
                                              kUpdateScreenWidth,
                                              kUpdateScreenHeight),
      checking_for_update_(true),
      maximal_curtain_time_(0),
      reboot_check_delay_(0) {
}

UpdateScreen::~UpdateScreen() {
  // Remove pointer to this object from view.
  if (view())
    view()->set_controller(NULL);
  CrosLibrary::Get()->GetUpdateLibrary()->RemoveObserver(this);
}

void UpdateScreen::UpdateStatusChanged(UpdateLibrary* library) {
  UpdateStatusOperation status = library->status().status;
  if (checking_for_update_ && status > UPDATE_STATUS_CHECKING_FOR_UPDATE) {
    checking_for_update_ = false;
  }

  switch (status) {
    case UPDATE_STATUS_CHECKING_FOR_UPDATE:
      // Do nothing in these cases, we don't want to notify the user of the
      // check unless there is an update.
      break;
    case UPDATE_STATUS_UPDATE_AVAILABLE:
      view()->SetProgress(kBeforeDownloadProgress);
      VLOG(1) << "Update available: " << library->status().new_version;
      break;
    case UPDATE_STATUS_DOWNLOADING:
      {
        view()->ShowCurtain(false);
        int download_progress = static_cast<int>(
            library->status().download_progress * kDownloadProgressIncrement);
        view()->SetProgress(kBeforeDownloadProgress + download_progress);
      }
      break;
    case UPDATE_STATUS_VERIFYING:
      view()->SetProgress(kBeforeVerifyingProgress);
      break;
    case UPDATE_STATUS_FINALIZING:
      view()->SetProgress(kBeforeFinalizingProgress);
      break;
    case UPDATE_STATUS_UPDATED_NEED_REBOOT:
      // Make sure that first OOBE stage won't be shown after reboot.
      WizardController::MarkOobeCompleted();
      view()->SetProgress(kProgressComplete);
      view()->ShowCurtain(false);
      CrosLibrary::Get()->GetUpdateLibrary()->RebootAfterUpdate();
      VLOG(1) << "Reboot API was called. Waiting for reboot.";
      reboot_timer_.Start(base::TimeDelta::FromSeconds(reboot_check_delay_),
                          this,
                          &UpdateScreen::OnWaitForRebootTimeElapsed);
      break;
    case UPDATE_STATUS_IDLE:
    case UPDATE_STATUS_ERROR:
    case UPDATE_STATUS_REPORTING_ERROR_EVENT:
      ExitUpdate();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void UpdateScreen::StartUpdate() {
  // Reset view.
  view()->Reset();
  view()->set_controller(this);

  // Start the maximal curtain time timer.
  if (maximal_curtain_time_ > 0) {
    maximal_curtain_time_timer_.Start(
        base::TimeDelta::FromSeconds(maximal_curtain_time_),
        this,
        &UpdateScreen::OnMaximalCurtainTimeElapsed);
  } else {
    view()->ShowCurtain(false);
  }

  view()->SetProgress(kBeforeUpdateCheckProgress);

  if (!CrosLibrary::Get()->EnsureLoaded()) {
    LOG(ERROR) << "Error loading CrosLibrary";
  } else {
    CrosLibrary::Get()->GetUpdateLibrary()->AddObserver(this);
    VLOG(1) << "Checking for update";
    if (!CrosLibrary::Get()->GetUpdateLibrary()->CheckForUpdate()) {
      ExitUpdate();
    }
  }
}

void UpdateScreen::CancelUpdate() {
#if !defined(OFFICIAL_BUILD)
  ExitUpdate();
#endif
}

void UpdateScreen::ExitUpdate() {
  maximal_curtain_time_timer_.Stop();
  ScreenObserver* observer = delegate()->GetObserver(this);

  if (!CrosLibrary::Get()->EnsureLoaded()) {
    observer->OnExit(ScreenObserver::UPDATE_ERROR_CHECKING_FOR_UPDATE);
  }

  UpdateLibrary* update_library = CrosLibrary::Get()->GetUpdateLibrary();
  update_library->RemoveObserver(this);
  switch (update_library->status().status) {
    case UPDATE_STATUS_IDLE:
      observer->OnExit(ScreenObserver::UPDATE_NOUPDATE);
      break;
    case UPDATE_STATUS_ERROR:
    case UPDATE_STATUS_REPORTING_ERROR_EVENT:
      observer->OnExit(checking_for_update_ ?
          ScreenObserver::UPDATE_ERROR_CHECKING_FOR_UPDATE :
          ScreenObserver::UPDATE_ERROR_UPDATING);
      break;
    default:
      NOTREACHED();
  }
}

void UpdateScreen::OnMaximalCurtainTimeElapsed() {
  view()->ShowCurtain(false);
}

void UpdateScreen::OnWaitForRebootTimeElapsed() {
  LOG(ERROR) << "Unable to reboot - asking user for a manual reboot.";
  view()->ShowManualRebootInfo();
}

void UpdateScreen::SetMaximalCurtainTime(int seconds) {
  if (seconds <= 0)
    maximal_curtain_time_timer_.Stop();
  DCHECK(!maximal_curtain_time_timer_.IsRunning());
  maximal_curtain_time_ = seconds;
}

void UpdateScreen::SetRebootCheckDelay(int seconds) {
  if (seconds <= 0)
    reboot_timer_.Stop();
  DCHECK(!reboot_timer_.IsRunning());
  reboot_check_delay_ = seconds;
}

}  // namespace chromeos
