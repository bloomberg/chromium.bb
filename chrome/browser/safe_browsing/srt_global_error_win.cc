// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_global_error_win.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/process/launch.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/srt_field_trial_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "components/component_updater/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using base::SingleThreadTaskRunner;
using base::ThreadTaskRunnerHandle;
using content::BrowserThread;

namespace safe_browsing {

namespace {

// Used as a backup plan in case the SRT executable was not successfully
// downloaded or run.
const char kSRTDownloadURL[] =
    "https://www.google.com/chrome/srt/?chrome-prompt=1";

// The extension to use to replace the temporary one created when the SRT was
// downloaded.
const base::FilePath::CharType kExecutableExtension[] = L"exe";

// A switch to add to the command line when executing the SRT.
const char kChromePromptSwitch[] = "chrome-prompt";

void MaybeExecuteSRTFromBlockingPool(
    const base::FilePath& downloaded_path,
    const scoped_refptr<SingleThreadTaskRunner>& task_runner,
    const base::Closure& success_callback,
    const base::Closure& failure_callback) {
  DCHECK(!downloaded_path.empty());

  if (base::PathExists(downloaded_path)) {
    base::FilePath executable_path(
        downloaded_path.ReplaceExtension(kExecutableExtension));
    if (base::ReplaceFile(downloaded_path, executable_path, NULL)) {
      base::CommandLine srt_command_line(executable_path);
      srt_command_line.AppendSwitch(kChromePromptSwitch);
      base::Process srt_process(
          base::LaunchProcess(srt_command_line, base::LaunchOptions()));
      if (srt_process.IsValid()) {
        task_runner->PostTask(FROM_HERE, success_callback);
        return;
      }
    }
  }

  task_runner->PostTask(FROM_HERE, failure_callback);
}

void DeleteFilesFromBlockingPool(const base::FilePath& downloaded_path) {
  base::DeleteFile(downloaded_path, false);
  base::DeleteFile(downloaded_path.ReplaceExtension(kExecutableExtension),
                   false);
}

}  // namespace

// SRTGlobalError ------------------------------------------------------------

SRTGlobalError::SRTGlobalError(GlobalErrorService* global_error_service,
                               const base::FilePath& downloaded_path)
    : global_error_service_(global_error_service),
      downloaded_path_(downloaded_path) {
  DCHECK(global_error_service_);
}

SRTGlobalError::~SRTGlobalError() {
  if (!interacted_) {
    BrowserThread::PostBlockingPoolTask(
        FROM_HERE, base::Bind(&DeleteFilesFromBlockingPool, downloaded_path_));
  }
}

bool SRTGlobalError::HasMenuItem() {
  return true;
}

int SRTGlobalError::MenuItemCommandID() {
  return IDC_SHOW_SRT_BUBBLE;
}

base::string16 SRTGlobalError::MenuItemLabel() {
  return l10n_util::GetStringUTF16(IDS_SRT_MENU_ITEM);
}

void SRTGlobalError::ExecuteMenuItem(Browser* browser) {
  RecordSRTPromptHistogram(SRT_PROMPT_SHOWN_FROM_MENU);
  show_dismiss_button_ = true;
  ShowBubbleView(browser);
}

void SRTGlobalError::ShowBubbleView(Browser* browser) {
  RecordSRTPromptHistogram(SRT_PROMPT_SHOWN);
  GlobalErrorWithStandardBubble::ShowBubbleView(browser);
}

bool SRTGlobalError::ShouldShowCloseButton() const {
  return true;
}

base::string16 SRTGlobalError::GetBubbleViewTitle() {
  return l10n_util::GetStringUTF16(IDS_SRT_BUBBLE_TITLE);
}

std::vector<base::string16> SRTGlobalError::GetBubbleViewMessages() {
  std::vector<base::string16> messages;
  messages.push_back(l10n_util::GetStringUTF16(IDS_SRT_BUBBLE_TEXT));
  return messages;
}

base::string16 SRTGlobalError::GetBubbleViewAcceptButtonLabel() {
  return downloaded_path_.empty()
             ? l10n_util::GetStringUTF16(IDS_SRT_BUBBLE_DOWNLOAD_BUTTON_TEXT)
             : l10n_util::GetStringUTF16(IDS_SRT_BUBBLE_RUN_BUTTON_TEXT);
}

bool SRTGlobalError::ShouldAddElevationIconToAcceptButton() {
  return !downloaded_path_.empty() && SRTPromptNeedsElevationIcon();
}

base::string16 SRTGlobalError::GetBubbleViewCancelButtonLabel() {
  if (show_dismiss_button_)
    return l10n_util::GetStringUTF16(IDS_SRT_BUBBLE_DISMISS);
  return base::string16();
}

void SRTGlobalError::OnBubbleViewDidClose(Browser* browser) {
  if (!interacted_) {
    // If user didn't interact with the bubble, it means they used the generic
    // close bubble button.
    RecordSRTPromptHistogram(SRT_PROMPT_CLOSED);
    g_browser_process->local_state()->SetBoolean(
        prefs::kSwReporterPendingPrompt, true);
  }
}

void SRTGlobalError::BubbleViewAcceptButtonPressed(Browser* browser) {
  RecordSRTPromptHistogram(SRT_PROMPT_ACCEPTED);
  interacted_ = true;
  global_error_service_->RemoveGlobalError(this);
  MaybeExecuteSRT();
}

void SRTGlobalError::BubbleViewCancelButtonPressed(Browser* browser) {
  RecordSRTPromptHistogram(SRT_PROMPT_DENIED);
  interacted_ = true;
  global_error_service_->RemoveGlobalError(this);

  BrowserThread::PostBlockingPoolTask(
      FROM_HERE, base::Bind(&DeleteFilesFromBlockingPool, downloaded_path_));
  OnUserinteractionDone();
}

bool SRTGlobalError::ShouldCloseOnDeactivate() const {
  return false;
}

void SRTGlobalError::MaybeExecuteSRT() {
  if (downloaded_path_.empty()) {
    FallbackToDownloadPage();
    return;
  }
  // At this point, this object owns itself, since ownership has been taken back
  // from the global_error_service_ in the call to RemoveGlobalError. This means
  // that it is safe to use base::Unretained here.
  BrowserThread::PostBlockingPoolTask(
      FROM_HERE, base::Bind(&MaybeExecuteSRTFromBlockingPool, downloaded_path_,
                            base::ThreadTaskRunnerHandle::Get(),
                            base::Bind(&SRTGlobalError::OnUserinteractionDone,
                                       base::Unretained(this)),
                            base::Bind(&SRTGlobalError::FallbackToDownloadPage,
                                       base::Unretained(this))));
}

void SRTGlobalError::FallbackToDownloadPage() {
  RecordSRTPromptHistogram(SRT_PROMPT_FALLBACK);

  Browser* browser = chrome::FindLastActive();
  if (browser) {
    browser->OpenURL(content::OpenURLParams(
        GURL(kSRTDownloadURL), content::Referrer(), NEW_FOREGROUND_TAB,
        ui::PAGE_TRANSITION_LINK, false));
  }

  BrowserThread::PostBlockingPoolTask(
      FROM_HERE, base::Bind(&DeleteFilesFromBlockingPool, downloaded_path_));
  OnUserinteractionDone();
}

void SRTGlobalError::OnUserinteractionDone() {
  DCHECK(interacted_);
  // Once the user interacted with the bubble, we can forget about any pending
  // prompt.
  g_browser_process->local_state()->SetBoolean(prefs::kSwReporterPendingPrompt,
                                               false);
  delete this;
}

}  // namespace safe_browsing
