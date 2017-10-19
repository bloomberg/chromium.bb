// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_impl_win.h"

#include <windows.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_fetcher_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_navigation_util_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_reboot_dialog_controller_impl_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_runner_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/settings_resetter_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_client_info_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/installer/util/scoped_token_privilege.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_status_code.h"
#include "ui/base/window_open_disposition.h"

namespace safe_browsing {

namespace {

using ::chrome_cleaner::mojom::ChromePrompt;
using ::chrome_cleaner::mojom::PromptAcceptance;
using ::content::BrowserThread;

// The global singleton instance. Exposed outside of GetInstance() so that it
// can be reset by tests.
ChromeCleanerControllerImpl* g_controller = nullptr;

// TODO(alito): Move these shared exit codes to the chrome_cleaner component.
// https://crbug.com/727956
constexpr int kRebootRequiredExitCode = 15;
constexpr int kRebootNotRequiredExitCode = 0;

// These values are used to send UMA information and are replicated in the
// enums.xml file, so the order MUST NOT CHANGE.
enum CleanupResultHistogramValue {
  CLEANUP_RESULT_SUCCEEDED = 0,
  CLEANUP_RESULT_REBOOT_REQUIRED = 1,
  CLEANUP_RESULT_FAILED = 2,

  CLEANUP_RESULT_MAX,
};

// These values are used to send UMA information and are replicated in the
// enums.xml file, so the order MUST NOT CHANGE.
enum IPCDisconnectedHistogramValue {
  IPC_DISCONNECTED_SUCCESS = 0,
  IPC_DISCONNECTED_LOST_WHILE_SCANNING = 1,
  IPC_DISCONNECTED_LOST_USER_PROMPTED = 2,

  IPC_DISCONNECTED_MAX,
};

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

void RecordCleanerLogsAcceptanceHistogram(bool logs_accepted) {
  UMA_HISTOGRAM_BOOLEAN("SoftwareReporter.CleanerLogsAcceptance",
                        logs_accepted);
}

void RecordCleanupResultHistogram(CleanupResultHistogramValue result) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.Cleaner.CleanupResult", result,
                            CLEANUP_RESULT_MAX);
}

void RecordIPCDisconnectedHistogram(IPCDisconnectedHistogramValue error) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.IPCDisconnected", error,
                            IPC_DISCONNECTED_MAX);
}

}  // namespace

void RecordCleanupStartedHistogram(CleanupStartedHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.CleanupStarted", value,
                            CLEANUP_STARTED_MAX);
}

ChromeCleanerControllerDelegate::ChromeCleanerControllerDelegate() = default;

ChromeCleanerControllerDelegate::~ChromeCleanerControllerDelegate() = default;

void ChromeCleanerControllerDelegate::FetchAndVerifyChromeCleaner(
    FetchedCallback fetched_callback) {
  FetchChromeCleaner(
      base::BindOnce(&OnChromeCleanerFetched, base::Passed(&fetched_callback)));
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

void ChromeCleanerControllerDelegate::StartRebootPromptFlow(
    ChromeCleanerController* controller) {
  // The controller object decides if and when a prompt should be shown.
  ChromeCleanerRebootDialogControllerImpl::Create(controller);
}

// static
ChromeCleanerControllerImpl* ChromeCleanerControllerImpl::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!g_controller)
    g_controller = new ChromeCleanerControllerImpl();

  return g_controller;
}

// static
ChromeCleanerController* ChromeCleanerController::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ChromeCleanerControllerImpl::GetInstance();
}

ChromeCleanerController::ChromeCleanerController() = default;

ChromeCleanerController::~ChromeCleanerController() = default;

bool ChromeCleanerControllerImpl::ShouldShowCleanupInSettingsUI() {
  return state_ == State::kInfected || state_ == State::kCleaning ||
         state_ == State::kRebootRequired ||
         (state_ == State::kIdle &&
          (idle_reason_ == IdleReason::kCleaningFailed ||
           idle_reason_ == IdleReason::kConnectionLost));
}

bool ChromeCleanerControllerImpl::IsPoweredByPartner() {
  // |reporter_invocation_| is not expected to hold a value for the entire
  // lifecycle of the ChromeCleanerController instance. To be consistent, use
  // a cached return value if the |reporter_invocation_| has been cleaned up.
  if (!reporter_invocation_) {
    return powered_by_partner_;
  }

  const std::string& reporter_engine =
      reporter_invocation_->command_line.GetSwitchValueASCII(
          chrome_cleaner::kEngineSwitch);
  // Currently, only engine=2 corresponds to a partner-powered engine. This
  // condition should be updated if other partner-powered engines are added.
  powered_by_partner_ = !reporter_engine.empty() && reporter_engine == "2";
  return powered_by_partner_;
}

ChromeCleanerController::State ChromeCleanerControllerImpl::state() const {
  return state_;
}

ChromeCleanerController::IdleReason ChromeCleanerControllerImpl::idle_reason()
    const {
  return idle_reason_;
}

void ChromeCleanerControllerImpl::SetLogsEnabled(bool logs_enabled) {
  if (logs_enabled_ == logs_enabled)
    return;

  logs_enabled_ = logs_enabled;
  for (auto& observer : observer_list_)
    observer.OnLogsEnabledChanged(logs_enabled_);
}

bool ChromeCleanerControllerImpl::logs_enabled() const {
  return logs_enabled_;
}

void ChromeCleanerControllerImpl::ResetIdleState() {
  if (state() != State::kIdle || idle_reason() == IdleReason::kInitial)
    return;

  idle_reason_ = IdleReason::kInitial;

  // SetStateAndNotifyObservers doesn't allow transitions to the same state.
  // Notify observers directly instead.
  for (auto& observer : observer_list_)
    NotifyObserver(&observer);
}

void ChromeCleanerControllerImpl::SetDelegateForTesting(
    ChromeCleanerControllerDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_ = delegate ? delegate : real_delegate_.get();
  DCHECK(delegate_);
}

// static
void ChromeCleanerControllerImpl::ResetInstanceForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (g_controller) {
    delete g_controller;
    g_controller = nullptr;
  }
}

void ChromeCleanerControllerImpl::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.AddObserver(observer);
  NotifyObserver(observer);
}

void ChromeCleanerControllerImpl::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.RemoveObserver(observer);
}

void ChromeCleanerControllerImpl::Scan(
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
      &ChromeCleanerControllerImpl::OnChromeCleanerFetchedAndVerified,
      base::Unretained(this)));
}

void ChromeCleanerControllerImpl::ReplyWithUserResponse(
    Profile* profile,
    UserResponse user_response) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state() != State::kInfected)
    return;

  DCHECK(prompt_user_callback_);

  PromptAcceptance acceptance = PromptAcceptance::DENIED;
  State new_state = State::kIdle;
  switch (user_response) {
    case UserResponse::kAcceptedWithLogs:
      acceptance = PromptAcceptance::ACCEPTED_WITH_LOGS;
      SetLogsEnabled(true);
      RecordCleanerLogsAcceptanceHistogram(true);
      new_state = State::kCleaning;
      delegate_->TagForResetting(profile);
      break;
    case UserResponse::kAcceptedWithoutLogs:
      acceptance = PromptAcceptance::ACCEPTED_WITHOUT_LOGS;
      SetLogsEnabled(false);
      RecordCleanerLogsAcceptanceHistogram(false);
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

  if (new_state == State::kCleaning)
    time_cleanup_started_ = base::Time::Now();

  // The transition to a new state should happen only after the response has
  // been posted on the UI thread so that if we transition to the kIdle state,
  // the response callback is not cleared before it has been posted.
  SetStateAndNotifyObservers(new_state);
}

void ChromeCleanerControllerImpl::Reboot() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state() != State::kRebootRequired)
    return;

  UMA_HISTOGRAM_BOOLEAN("SoftwareReporter.Cleaner.RebootResponse", true);
  InitiateReboot();
}

ChromeCleanerControllerImpl::ChromeCleanerControllerImpl()
    : real_delegate_(base::MakeUnique<ChromeCleanerControllerDelegate>()),
      delegate_(real_delegate_.get()),
      weak_factory_(this) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

ChromeCleanerControllerImpl::~ChromeCleanerControllerImpl() = default;

void ChromeCleanerControllerImpl::NotifyObserver(Observer* observer) const {
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

void ChromeCleanerControllerImpl::SetStateAndNotifyObservers(State state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(state_, state);

  state_ = state;

  if (state_ == State::kIdle || state_ == State::kRebootRequired)
    ResetCleanerDataAndInvalidateWeakPtrs();

  for (auto& observer : observer_list_)
    NotifyObserver(&observer);
}

void ChromeCleanerControllerImpl::ResetCleanerDataAndInvalidateWeakPtrs() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  weak_factory_.InvalidateWeakPtrs();
  reporter_invocation_.reset();
  files_to_delete_.reset();
  prompt_user_callback_.Reset();
}

void ChromeCleanerControllerImpl::OnChromeCleanerFetchedAndVerified(
    base::FilePath executable_path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(State::kScanning, state());
  DCHECK(reporter_invocation_);

  if (executable_path.empty()) {
    idle_reason_ = IdleReason::kScanningFailed;
    SetStateAndNotifyObservers(State::kIdle);
    RecordPromptNotShownWithReasonHistogram(
        NO_PROMPT_REASON_CLEANER_DOWNLOAD_FAILED);
    return;
  }

  DCHECK(executable_path.MatchesExtension(FILE_PATH_LITERAL(".exe")));

  ChromeCleanerRunner::ChromeMetricsStatus metrics_status =
      delegate_->IsMetricsAndCrashReportingEnabled()
          ? ChromeCleanerRunner::ChromeMetricsStatus::kEnabled
          : ChromeCleanerRunner::ChromeMetricsStatus::kDisabled;

  ChromeCleanerRunner::RunChromeCleanerAndReplyWithExitCode(
      executable_path, *reporter_invocation_, metrics_status,
      base::Bind(&ChromeCleanerControllerImpl::WeakOnPromptUser,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeCleanerControllerImpl::OnConnectionClosed,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeCleanerControllerImpl::OnCleanerProcessDone,
                 weak_factory_.GetWeakPtr()),
      // Our callbacks should be dispatched to the UI thread only.
      base::ThreadTaskRunnerHandle::Get());

  time_scanning_started_ = base::Time::Now();
}

// static
void ChromeCleanerControllerImpl::WeakOnPromptUser(
    const base::WeakPtr<ChromeCleanerControllerImpl>& controller,
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

void ChromeCleanerControllerImpl::OnPromptUser(
    std::unique_ptr<std::set<base::FilePath>> files_to_delete,
    ChromePrompt::PromptUserCallback prompt_user_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(files_to_delete);
  DCHECK_EQ(State::kScanning, state());
  DCHECK(!files_to_delete_);
  DCHECK(!prompt_user_callback_);
  DCHECK(!time_scanning_started_.is_null());

  UMA_HISTOGRAM_LONG_TIMES_100("SoftwareReporter.Cleaner.ScanningTime",
                               base::Time::Now() - time_scanning_started_);

  if (files_to_delete->empty()) {
    BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)
        ->PostTask(FROM_HERE, base::BindOnce(std::move(prompt_user_callback),
                                             PromptAcceptance::DENIED));
    idle_reason_ = IdleReason::kScanningFoundNothing;
    SetStateAndNotifyObservers(State::kIdle);
    RecordPromptNotShownWithReasonHistogram(NO_PROMPT_REASON_NOTHING_FOUND);
    return;
  }

  UMA_HISTOGRAM_COUNTS_1000("SoftwareReporter.NumberOfFilesToDelete",
                            files_to_delete->size());
  files_to_delete_ = std::move(files_to_delete);
  prompt_user_callback_ = std::move(prompt_user_callback);
  SetStateAndNotifyObservers(State::kInfected);
}

void ChromeCleanerControllerImpl::OnConnectionClosed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(State::kIdle, state());
  DCHECK_NE(State::kRebootRequired, state());

  if (state() == State::kScanning || state() == State::kInfected) {
    if (state() == State::kScanning) {
      RecordPromptNotShownWithReasonHistogram(
          NO_PROMPT_REASON_IPC_CONNECTION_BROKEN);
      RecordIPCDisconnectedHistogram(IPC_DISCONNECTED_LOST_WHILE_SCANNING);
    } else {
      RecordIPCDisconnectedHistogram(IPC_DISCONNECTED_LOST_USER_PROMPTED);
    }

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
  RecordIPCDisconnectedHistogram(IPC_DISCONNECTED_SUCCESS);
}

void ChromeCleanerControllerImpl::OnCleanerProcessDone(
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

  if (process_status.launch_status ==
      ChromeCleanerRunner::LaunchStatus::kSuccess) {
    if (process_status.exit_code == kRebootRequiredExitCode ||
        process_status.exit_code == kRebootNotRequiredExitCode) {
      DCHECK(!time_cleanup_started_.is_null());
      UMA_HISTOGRAM_CUSTOM_TIMES("SoftwareReporter.Cleaner.CleaningTime",
                                 base::Time::Now() - time_cleanup_started_,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromHours(5), 100);
    }

    if (process_status.exit_code == kRebootRequiredExitCode) {
      RecordCleanupResultHistogram(CLEANUP_RESULT_REBOOT_REQUIRED);
      SetStateAndNotifyObservers(State::kRebootRequired);

      // Start the reboot prompt flow.
      delegate_->StartRebootPromptFlow(this);
      return;
    }

    if (process_status.exit_code == kRebootNotRequiredExitCode) {
      RecordCleanupResultHistogram(CLEANUP_RESULT_SUCCEEDED);
      delegate_->ResetTaggedProfiles(
          g_browser_process->profile_manager()->GetLoadedProfiles(),
          base::BindOnce(&base::DoNothing));
      idle_reason_ = IdleReason::kCleaningSucceeded;
      SetStateAndNotifyObservers(State::kIdle);
      return;
    }
  }

  RecordCleanupResultHistogram(CLEANUP_RESULT_FAILED);
  idle_reason_ = IdleReason::kCleaningFailed;
  SetStateAndNotifyObservers(State::kIdle);
}

void ChromeCleanerControllerImpl::InitiateReboot() {
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

}  // namespace safe_browsing
