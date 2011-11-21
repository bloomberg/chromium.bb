// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/update_screen.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/update_screen_actor.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

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

// Invoked from call to RequestUpdateCheck upon completion of the DBus call.
void StartUpdateCallback(void* user_data,
                         UpdateResult result,
                         const char* msg) {
  VLOG(1) << "Callback from RequestUpdateCheck, result " << result;
  DCHECK(user_data);
  UpdateScreen* screen = static_cast<UpdateScreen*>(user_data);
  if (UpdateScreen::HasInstance(screen)) {
    if (result == chromeos::UPDATE_RESULT_SUCCESS)
      screen->SetIgnoreIdleStatus(false);
    else
      screen->ExitUpdate(UpdateScreen::REASON_UPDATE_INIT_FAILED);
  }
}

}  // anonymous namespace

// static
UpdateScreen::InstanceSet& UpdateScreen::GetInstanceSet() {
  CR_DEFINE_STATIC_LOCAL(std::set<UpdateScreen*>, instance_set, ());
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));  // not threadsafe.
  return instance_set;
}

// static
bool UpdateScreen::HasInstance(UpdateScreen* inst) {
  InstanceSet& instance_set = GetInstanceSet();
  InstanceSet::iterator found = instance_set.find(inst);
  return (found != instance_set.end());
}

UpdateScreen::UpdateScreen(ScreenObserver* screen_observer,
                           UpdateScreenActor* actor)
    : WizardScreen(screen_observer),
      reboot_check_delay_(0),
      is_checking_for_update_(true),
      is_downloading_update_(false),
      is_ignore_update_deadlines_(false),
      is_shown_(false),
      ignore_idle_status_(true),
      actor_(actor) {
  actor_->SetDelegate(this);
  GetInstanceSet().insert(this);
}

UpdateScreen::~UpdateScreen() {
  CrosLibrary::Get()->GetUpdateLibrary()->RemoveObserver(this);
  GetInstanceSet().erase(this);
  if (actor_)
    actor_->SetDelegate(NULL);
}

void UpdateScreen::UpdateStatusChanged(const UpdateLibrary::Status& status) {
  if (is_checking_for_update_ &&
      status.status > UPDATE_STATUS_CHECKING_FOR_UPDATE) {
    is_checking_for_update_ = false;
  }
  if (ignore_idle_status_ && status.status > UPDATE_STATUS_IDLE) {
    ignore_idle_status_ = false;
  }

  switch (status.status) {
    case UPDATE_STATUS_CHECKING_FOR_UPDATE:
      // Do nothing in these cases, we don't want to notify the user of the
      // check unless there is an update.
      break;
    case UPDATE_STATUS_UPDATE_AVAILABLE:
      MakeSureScreenIsShown();
      actor_->SetProgress(kBeforeDownloadProgress);
      if (!HasCriticalUpdate()) {
        LOG(INFO) << "Noncritical update available: "
                  << status.new_version;
        ExitUpdate(REASON_UPDATE_NON_CRITICAL);
      } else {
        LOG(INFO) << "Critical update available: "
                  << status.new_version;
        actor_->ShowPreparingUpdatesInfo(true);
        actor_->ShowCurtain(false);
      }
      break;
    case UPDATE_STATUS_DOWNLOADING:
      {
        MakeSureScreenIsShown();
        if (!is_downloading_update_) {
          // Because update engine doesn't send UPDATE_STATUS_UPDATE_AVAILABLE
          // we need to is update critical on first downloading notification.
          is_downloading_update_ = true;
          if (!HasCriticalUpdate()) {
            LOG(INFO) << "Non-critical update available: "
                      << status.new_version;
            ExitUpdate(REASON_UPDATE_NON_CRITICAL);
          } else {
            LOG(INFO) << "Critical update available: "
                      << status.new_version;
            actor_->ShowPreparingUpdatesInfo(false);
            actor_->ShowCurtain(false);
          }
        }
        int download_progress = static_cast<int>(
            status.download_progress * kDownloadProgressIncrement);
        actor_->SetProgress(kBeforeDownloadProgress + download_progress);
      }
      break;
    case UPDATE_STATUS_VERIFYING:
      MakeSureScreenIsShown();
      actor_->SetProgress(kBeforeVerifyingProgress);
      break;
    case UPDATE_STATUS_FINALIZING:
      MakeSureScreenIsShown();
      actor_->SetProgress(kBeforeFinalizingProgress);
      break;
    case UPDATE_STATUS_UPDATED_NEED_REBOOT:
      MakeSureScreenIsShown();
      // Make sure that first OOBE stage won't be shown after reboot.
      WizardController::MarkOobeCompleted();
      actor_->SetProgress(kProgressComplete);
      if (HasCriticalUpdate()) {
        actor_->ShowCurtain(false);
        VLOG(1) << "Initiate reboot after update";
        CrosLibrary::Get()->GetUpdateLibrary()->RebootAfterUpdate();
        reboot_timer_.Start(FROM_HERE,
                            base::TimeDelta::FromSeconds(reboot_check_delay_),
                            this,
                            &UpdateScreen::OnWaitForRebootTimeElapsed);
      } else {
        ExitUpdate(REASON_UPDATE_NON_CRITICAL);
      }
      break;
    case UPDATE_STATUS_IDLE:
      if (ignore_idle_status_) {
        // It is first IDLE status that is sent before we initiated the check.
        break;
      }
      // else no break

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
  CrosLibrary::Get()->GetUpdateLibrary()->AddObserver(this);
  VLOG(1) << "Initiate update check";
  CrosLibrary::Get()->GetUpdateLibrary()->RequestUpdateCheck(
      StartUpdateCallback, this);
}

void UpdateScreen::CancelUpdate() {
  VLOG(1) << "Forced update cancel";
  ExitUpdate(REASON_UPDATE_CANCELED);
}

void UpdateScreen::Show() {
  is_shown_ = true;
  actor_->Show();
  actor_->SetProgress(kBeforeUpdateCheckProgress);
}

void UpdateScreen::Hide() {
  actor_->Hide();
  is_shown_ = false;
}

void UpdateScreen::PrepareToShow() {
  actor_->PrepareToShow();
}

void UpdateScreen::ExitUpdate(UpdateScreen::ExitReason reason) {
  CrosLibrary::Get()->GetUpdateLibrary()->RemoveObserver(this);

  switch (reason) {
    case REASON_UPDATE_CANCELED:
      get_screen_observer()->OnExit(ScreenObserver::UPDATE_NOUPDATE);
      break;
    case REASON_UPDATE_INIT_FAILED:
      get_screen_observer()->OnExit(
          ScreenObserver::UPDATE_ERROR_CHECKING_FOR_UPDATE);
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
            get_screen_observer()->OnExit(ScreenObserver::UPDATE_NOUPDATE);
            break;
          case UPDATE_STATUS_ERROR:
          case UPDATE_STATUS_REPORTING_ERROR_EVENT:
            get_screen_observer()->OnExit(is_checking_for_update_ ?
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
  MakeSureScreenIsShown();
  actor_->ShowManualRebootInfo();
}

void UpdateScreen::MakeSureScreenIsShown() {
  if (!is_shown_)
    get_screen_observer()->ShowCurrentScreen();
}

void UpdateScreen::SetRebootCheckDelay(int seconds) {
  if (seconds <= 0)
    reboot_timer_.Stop();
  DCHECK(!reboot_timer_.IsRunning());
  reboot_check_delay_ = seconds;
}

void UpdateScreen::SetIgnoreIdleStatus(bool ignore_idle_status) {
  ignore_idle_status_ = ignore_idle_status;
}

bool UpdateScreen::HasCriticalUpdate() {
  if (is_ignore_update_deadlines_)
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

void UpdateScreen::OnActorDestroyed(UpdateScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

}  // namespace chromeos
