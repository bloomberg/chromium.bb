// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/update_screen.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
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

const char kUpdateDeadlineFile[] = "/tmp/update-check-response-deadline";

}  // anonymous namespace

namespace chromeos {

UpdateScreen::UpdateScreen(WizardScreenDelegate* delegate)
    : DefaultViewScreen<chromeos::UpdateView>(delegate,
                                              kUpdateScreenWidth,
                                              kUpdateScreenHeight),
      checking_for_update_(true),
      reboot_check_delay_(0),
      is_downloading_update_(false),
      is_all_updates_critical_(false) {
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
      if (!HasCriticalUpdate()) {
        LOG(INFO) << "Noncritical update available: "
                  << library->status().new_version;
        ExitUpdate(REASON_UPDATE_NON_CRITICAL);
      } else {
        LOG(INFO) << "Critical update available: "
                  << library->status().new_version;
      }
      break;
    case UPDATE_STATUS_DOWNLOADING:
      {
        if (!is_downloading_update_) {
          // Because update engine doesn't send UPDATE_STATUS_UPDATE_AVAILABLE
          // we need to is update critical on first downloading notification.
          is_downloading_update_ = true;
          if (!HasCriticalUpdate()) {
            LOG(INFO) << "Non-critical update available: "
                      << library->status().new_version;
            ExitUpdate(REASON_UPDATE_NON_CRITICAL);
          } else {
            LOG(INFO) << "Critical update available: "
                      << library->status().new_version;
          }
        }
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
      if (HasCriticalUpdate()) {
        view()->ShowCurtain(false);
        VLOG(1) << "Initiate reboot after update";
        CrosLibrary::Get()->GetUpdateLibrary()->RebootAfterUpdate();
        reboot_timer_.Start(base::TimeDelta::FromSeconds(reboot_check_delay_),
                            this,
                            &UpdateScreen::OnWaitForRebootTimeElapsed);
      } else {
        ExitUpdate(REASON_UPDATE_NON_CRITICAL);
      }
      break;
    case UPDATE_STATUS_IDLE:
    case UPDATE_STATUS_ERROR:
    case UPDATE_STATUS_REPORTING_ERROR_EVENT:
      ExitUpdate(REASON_UPDATE_ENDED);
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
  is_downloading_update_ = false;
  view()->SetProgress(kBeforeUpdateCheckProgress);

  if (!CrosLibrary::Get()->EnsureLoaded()) {
    LOG(ERROR) << "Error loading CrosLibrary";
    ExitUpdate(REASON_UPDATE_INIT_FAILED);
  } else {
    CrosLibrary::Get()->GetUpdateLibrary()->AddObserver(this);
    VLOG(1) << "Initiate update check";
    if (!CrosLibrary::Get()->GetUpdateLibrary()->CheckForUpdate()) {
      ExitUpdate(REASON_UPDATE_INIT_FAILED);
    }
  }
}

void UpdateScreen::CancelUpdate() {
  // Screen has longer lifetime than it's view.
  // View is deleted after wizard proceeds to the next screen.
  if (view())
    ExitUpdate(REASON_UPDATE_CANCELED);
}

void UpdateScreen::ExitUpdate(UpdateScreen::ExitReason reason) {
  ScreenObserver* observer = delegate()->GetObserver(this);
  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetUpdateLibrary()->RemoveObserver(this);

  switch(reason) {
    case REASON_UPDATE_CANCELED:
      observer->OnExit(ScreenObserver::UPDATE_NOUPDATE);
      break;
    case REASON_UPDATE_INIT_FAILED:
      observer->OnExit(ScreenObserver::UPDATE_ERROR_CHECKING_FOR_UPDATE);
      break;
    case REASON_UPDATE_NON_CRITICAL:
    case REASON_UPDATE_ENDED:
      {
        UpdateLibrary* update_library = CrosLibrary::Get()->GetUpdateLibrary();
        switch (update_library->status().status) {
          case UPDATE_STATUS_UPDATE_AVAILABLE:
          case UPDATE_STATUS_UPDATED_NEED_REBOOT:
          case UPDATE_STATUS_DOWNLOADING:
          case UPDATE_STATUS_FINALIZING:
          case UPDATE_STATUS_VERIFYING:
            DCHECK(!HasCriticalUpdate());
            // Noncritical update, just exit screen as if there is no update.
            // no break
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
      break;
    default:
      NOTREACHED();
  }
}

void UpdateScreen::OnWaitForRebootTimeElapsed() {
  LOG(ERROR) << "Unable to reboot - asking user for a manual reboot.";
  view()->ShowManualRebootInfo();
}

void UpdateScreen::SetRebootCheckDelay(int seconds) {
  if (seconds <= 0)
    reboot_timer_.Stop();
  DCHECK(!reboot_timer_.IsRunning());
  reboot_check_delay_ = seconds;
}

bool UpdateScreen::HasCriticalUpdate() {
  if (is_all_updates_critical_)
    return true;

  std::string deadline;
  // Checking for update flag file causes us to do blocking IO on UI thread.
  // Temporarily allow it until we fix http://crosbug.com/11106
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  FilePath update_deadline_file_path(kUpdateDeadlineFile);
  if (!file_util::ReadFileToString(update_deadline_file_path, &deadline) ||
      deadline.empty()) {
    return false;
  }

  // TODO(dpolukhin): Analyze file content. Now we can just assume that
  // if the file exists and not empty, there is critical update.
  return true;
}

void UpdateScreen::SetAllUpdatesCritical(bool is_critical) {
  is_all_updates_critical_ = is_critical;
}

}  // namespace chromeos
