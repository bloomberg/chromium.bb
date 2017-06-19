// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"

#include <windows.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_fetcher_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_runner_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/settings_resetter_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_client_info_win.h"
#include "chrome/installer/util/scoped_token_privilege.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_status_code.h"

namespace safe_browsing {

namespace {

using ::chrome_cleaner::mojom::ChromePrompt;
using ::chrome_cleaner::mojom::PromptAcceptance;
using ::content::BrowserThread;

// Keeps track of whether GetInstance() has been called.
bool g_instance_exists = false;

// TODO(alito): Move these shared exit codes to the chrome_cleaner component.
// https://crbug.com/727956
constexpr int kRebootRequiredExitCode = 15;
constexpr int kRebootNotRequiredExitCode = 0;

// Attempts to change the Chrome Cleaner binary's suffix to ".exe". Will return
// an empty FilePath on failure. Should be called on a sequence with traits
// appropriate for IO operations.
base::FilePath VerifyAndRenameDownloadedCleaner(
    base::FilePath downloaded_path,
    ChromeCleanerFetchStatus fetch_status) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (downloaded_path.empty() || !base::PathExists(downloaded_path))
    return base::FilePath();

  if (fetch_status != ChromeCleanerFetchStatus::kSuccess) {
    base::DeleteFile(downloaded_path, /*recursive=*/false);
    return base::FilePath();
  }

  base::FilePath executable_path(
      downloaded_path.ReplaceExtension(FILE_PATH_LITERAL("exe")));

  if (!base::ReplaceFile(downloaded_path, executable_path, nullptr)) {
    base::DeleteFile(downloaded_path, /*recursive=*/false);
    return base::FilePath();
  }

  return executable_path;
}

void OnChromeCleanerFetched(
    ChromeCleanerControllerDelegate::FetchedCallback fetched_callback,
    base::FilePath downloaded_path,
    ChromeCleanerFetchStatus fetch_status) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BACKGROUND,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(VerifyAndRenameDownloadedCleaner, downloaded_path,
                     fetch_status),
      std::move(fetched_callback));
}

ChromeCleanerController::IdleReason IdleReasonWhenConnectionClosedTooSoon(
    ChromeCleanerController::State current_state) {
  DCHECK(current_state == ChromeCleanerController::State::kScanning ||
         current_state == ChromeCleanerController::State::kInfected);

  return current_state == ChromeCleanerController::State::kScanning
             ? ChromeCleanerController::IdleReason::kScanningFailed
             : ChromeCleanerController::IdleReason::kConnectionLost;
}

}  // namespace

ChromeCleanerControllerDelegate::ChromeCleanerControllerDelegate() = default;

ChromeCleanerControllerDelegate::~ChromeCleanerControllerDelegate() = default;

void ChromeCleanerControllerDelegate::FetchAndVerifyChromeCleaner(
    FetchedCallback fetched_callback) {
  FetchChromeCleaner(
      base::BindOnce(&OnChromeCleanerFetched, base::Passed(&fetched_callback)));
}

bool ChromeCleanerControllerDelegate::
    SafeBrowsingExtendedReportingScoutEnabled() {
  return safe_browsing::SafeBrowsingExtendedReportingScoutEnabled();
}

bool ChromeCleanerControllerDelegate::IsMetricsAndCrashReportingEnabled() {
  return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
}

void ChromeCleanerControllerDelegate::TagForResetting(Profile* profile) {
  if (PostCleanupSettingsResetter::IsEnabled())
    PostCleanupSettingsResetter().TagForResetting(profile);
}

void ChromeCleanerControllerDelegate::ResetTaggedProfiles(
    std::vector<Profile*> profiles,
    base::OnceClosure continuation) {
  if (PostCleanupSettingsResetter::IsEnabled()) {
    PostCleanupSettingsResetter().ResetTaggedProfiles(
        std::move(profiles), std::move(continuation),
        base::MakeUnique<PostCleanupSettingsResetter::Delegate>());
  }
}

// static
ChromeCleanerController* ChromeCleanerController::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  static ChromeCleanerController* const kInstance =
      new ChromeCleanerController();
  g_instance_exists = true;
  return kInstance;
}

// static
bool ChromeCleanerController::ShouldShowCleanupInSettingsUI() {
  // Short-circuit if the instance doesn't exist to avoid creating it during
  // navigation to chrome://settings.
  return g_instance_exists && GetInstance()->state() == State::kInfected;
}

void ChromeCleanerController::SetDelegateForTesting(
    ChromeCleanerControllerDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_ = delegate ? delegate : real_delegate_.get();
  DCHECK(delegate_);
}

void ChromeCleanerController::DismissRebootForTesting() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(State::kRebootRequired, state());
  state_ = State::kIdle;
}

void ChromeCleanerController::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.AddObserver(observer);
  NotifyObserver(observer);
}

void ChromeCleanerController::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.RemoveObserver(observer);
}

void ChromeCleanerController::Scan(
    const SwReporterInvocation& reporter_invocation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(reporter_invocation.BehaviourIsSupported(
      SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT));

  if (state() != State::kIdle)
    return;

  DCHECK(!reporter_invocation_);
  reporter_invocation_ =
      base::MakeUnique<SwReporterInvocation>(reporter_invocation);
  SetStateAndNotifyObservers(State::kScanning);
  // base::Unretained is safe because the ChromeCleanerController instance is
  // guaranteed to outlive the UI thread.
  delegate_->FetchAndVerifyChromeCleaner(base::BindOnce(
      &ChromeCleanerController::OnChromeCleanerFetchedAndVerified,
      base::Unretained(this)));
}

void ChromeCleanerController::ReplyWithUserResponse(
    Profile* profile,
    UserResponse user_response) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(prompt_user_callback_);

  if (state() != State::kInfected)
    return;

  PromptAcceptance acceptance = PromptAcceptance::DENIED;
  State new_state = State::kIdle;
  switch (user_response) {
    case UserResponse::kAccepted:
      acceptance = PromptAcceptance::ACCEPTED;
      new_state = State::kCleaning;
      delegate_->TagForResetting(profile);
      break;
    case UserResponse::kDenied:  // Fallthrough
    case UserResponse::kDismissed:
      acceptance = PromptAcceptance::DENIED;
      idle_reason_ = IdleReason::kUserDeclinedCleanup;
      new_state = State::kIdle;
      break;
  }

  BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)
      ->PostTask(FROM_HERE,
                 base::BindOnce(std::move(prompt_user_callback_), acceptance));

  // The transition to a new state should happen only after the response has
  // been posted on the UI thread so that if we transition to the kIdle state,
  // the response callback is not cleared before it has been posted.
  SetStateAndNotifyObservers(new_state);
}

void ChromeCleanerController::Reboot() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state() != State::kRebootRequired)
    return;

  InitiateReboot();
}

ChromeCleanerController::ChromeCleanerController()
    : real_delegate_(base::MakeUnique<ChromeCleanerControllerDelegate>()),
      delegate_(real_delegate_.get()),
      weak_factory_(this) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

ChromeCleanerController::~ChromeCleanerController() = default;

void ChromeCleanerController::NotifyObserver(Observer* observer) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  switch (state_) {
    case State::kIdle:
      observer->OnIdle(idle_reason_);
      break;
    case State::kScanning:
      observer->OnScanning();
      break;
    case State::kInfected:
      observer->OnInfected(*files_to_delete_);
      break;
    case State::kCleaning:
      observer->OnCleaning(*files_to_delete_);
      break;
    case State::kRebootRequired:
      observer->OnRebootRequired();
      break;
  }
}

void ChromeCleanerController::SetStateAndNotifyObservers(State state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(state_, state);

  state_ = state;

  if (state_ == State::kIdle || state_ == State::kRebootRequired)
    ResetCleanerDataAndInvalidateWeakPtrs();

  for (auto& observer : observer_list_)
    NotifyObserver(&observer);
}

void ChromeCleanerController::ResetCleanerDataAndInvalidateWeakPtrs() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  weak_factory_.InvalidateWeakPtrs();
  reporter_invocation_.reset();
  files_to_delete_.reset();
  prompt_user_callback_.Reset();
}

void ChromeCleanerController::OnChromeCleanerFetchedAndVerified(
    base::FilePath executable_path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(State::kScanning, state());
  DCHECK(reporter_invocation_);

  if (executable_path.empty()) {
    idle_reason_ = IdleReason::kScanningFailed;
    SetStateAndNotifyObservers(State::kIdle);
    return;
  }

  DCHECK(executable_path.MatchesExtension(FILE_PATH_LITERAL(".exe")));

  ChromeCleanerRunner::ChromeMetricsStatus metrics_status =
      delegate_->IsMetricsAndCrashReportingEnabled()
          ? ChromeCleanerRunner::ChromeMetricsStatus::kEnabled
          : ChromeCleanerRunner::ChromeMetricsStatus::kDisabled;

  ChromeCleanerRunner::CleanerLogsStatus cleaner_logs_status =
      delegate_->SafeBrowsingExtendedReportingScoutEnabled()
          ? ChromeCleanerRunner::CleanerLogsStatus::kUploadEnabled
          : ChromeCleanerRunner::CleanerLogsStatus::kUploadDisabled;

  ChromeCleanerRunner::RunChromeCleanerAndReplyWithExitCode(
      executable_path, *reporter_invocation_, metrics_status,
      cleaner_logs_status,
      base::Bind(&ChromeCleanerController::WeakOnPromptUser,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeCleanerController::OnConnectionClosed,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeCleanerController::OnCleanerProcessDone,
                 weak_factory_.GetWeakPtr()),
      // Our callbacks should be dispatched to the UI thread only.
      base::ThreadTaskRunnerHandle::Get());
}

// static
void ChromeCleanerController::WeakOnPromptUser(
    const base::WeakPtr<ChromeCleanerController>& controller,
    std::unique_ptr<std::set<base::FilePath>> files_to_delete,
    ChromePrompt::PromptUserCallback prompt_user_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the weak pointer has been invalidated, the controller is no longer able
  // to receive callbacks, so respond with PromptAcceptance::Denied immediately.
  if (!controller) {
    BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)
        ->PostTask(FROM_HERE, base::BindOnce(std::move(prompt_user_callback),
                                             PromptAcceptance::DENIED));
  }

  controller->OnPromptUser(std::move(files_to_delete),
                           std::move(prompt_user_callback));
}

void ChromeCleanerController::OnPromptUser(
    std::unique_ptr<std::set<base::FilePath>> files_to_delete,
    ChromePrompt::PromptUserCallback prompt_user_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(files_to_delete);
  DCHECK_EQ(State::kScanning, state());
  DCHECK(!files_to_delete_);
  DCHECK(!prompt_user_callback_);

  if (files_to_delete->empty()) {
    BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)
        ->PostTask(FROM_HERE, base::BindOnce(std::move(prompt_user_callback),
                                             PromptAcceptance::DENIED));
    idle_reason_ = IdleReason::kScanningFoundNothing;
    SetStateAndNotifyObservers(State::kIdle);
    return;
  }

  files_to_delete_ = std::move(files_to_delete);
  prompt_user_callback_ = std::move(prompt_user_callback);
  SetStateAndNotifyObservers(State::kInfected);
}

void ChromeCleanerController::OnConnectionClosed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(State::kIdle, state());
  DCHECK_NE(State::kRebootRequired, state());

  if (state() == State::kScanning || state() == State::kInfected) {
    idle_reason_ = IdleReasonWhenConnectionClosedTooSoon(state());
    SetStateAndNotifyObservers(State::kIdle);
    return;
  }
  // Nothing to do if OnConnectionClosed() is called in other states:
  // - This function will not be called in the kIdle and kRebootRequired
  //   states since we invalidate all weak pointers when we enter those states.
  // - In the kCleaning state, we don't care about the connection to the Chrome
  //   Cleaner process since communication via Mojo is complete and only the
  //   exit code of the process is of any use to us (for deciding whether we
  //   need to reboot).
}

void ChromeCleanerController::OnCleanerProcessDone(
    ChromeCleanerRunner::ProcessStatus process_status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state() == State::kScanning || state() == State::kInfected) {
    idle_reason_ = IdleReasonWhenConnectionClosedTooSoon(state());
    SetStateAndNotifyObservers(State::kIdle);
    return;
  }

  DCHECK_EQ(State::kCleaning, state());
  DCHECK_NE(ChromeCleanerRunner::LaunchStatus::kLaunchFailed,
            process_status.launch_status);

  if (process_status.launch_status !=
      ChromeCleanerRunner::LaunchStatus::kSuccess) {
    idle_reason_ = IdleReason::kCleaningFailed;
    SetStateAndNotifyObservers(State::kIdle);
    return;
  }

  if (process_status.exit_code == kRebootRequiredExitCode) {
    SetStateAndNotifyObservers(State::kRebootRequired);
    return;
  }

  if (process_status.exit_code == kRebootNotRequiredExitCode) {
    delegate_->ResetTaggedProfiles(
        g_browser_process->profile_manager()->GetLoadedProfiles(),
        base::BindOnce(&ChromeCleanerController::OnSettingsResetCompleted,
                       base::Unretained(this)));
    ResetCleanerDataAndInvalidateWeakPtrs();
    return;
  }

  idle_reason_ = IdleReason::kCleaningFailed;
  SetStateAndNotifyObservers(State::kIdle);
}

void ChromeCleanerController::InitiateReboot() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  installer::ScopedTokenPrivilege scoped_se_shutdown_privilege(
      SE_SHUTDOWN_NAME);
  if (!scoped_se_shutdown_privilege.is_enabled() ||
      !::ExitWindowsEx(EWX_REBOOT, SHTDN_REASON_MAJOR_SOFTWARE |
                                       SHTDN_REASON_MINOR_OTHER |
                                       SHTDN_REASON_FLAG_PLANNED)) {
    for (auto& observer : observer_list_)
      observer.OnRebootFailed();
  }
}

void ChromeCleanerController::OnSettingsResetCompleted() {
  idle_reason_ = IdleReason::kCleaningSucceeded;
  SetStateAndNotifyObservers(State::kIdle);
}

}  // namespace safe_browsing
