// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/safe_audio_video_checker.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/utility_process_host.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "ui/base/l10n/l10n_util.h"

SafeAudioVideoChecker::SafeAudioVideoChecker(
    base::File file,
    const storage::CopyOrMoveFileValidator::ResultCallback& callback)
    : state_(INITIAL_STATE), file_(std::move(file)), callback_(callback) {
  DCHECK(!callback.is_null());
}

void SafeAudioVideoChecker::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (state_ != INITIAL_STATE)
    return;
  state_ = STARTED_STATE;

  if (!file_.IsValid()) {
    callback_.Run(base::File::FILE_ERROR_SECURITY);
    state_ = FINISHED_STATE;
    return;
  }

  IPC::PlatformFileForTransit file_for_transit =
      IPC::TakePlatformFileForTransit(std::move(file_));
  if (file_for_transit == IPC::InvalidPlatformFileForTransit()) {
    OnCheckingFinished(false /* valid? */);
    return;
  }

  utility_process_host_ = content::UtilityProcessHost::Create(
      this, base::ThreadTaskRunnerHandle::Get())->AsWeakPtr();
  utility_process_host_->SetName(l10n_util::GetStringUTF16(
      IDS_UTILITY_PROCESS_MEDIA_FILE_CHECKER_NAME));

  const int64_t kFileDecodeTimeInMS = 250;
  utility_process_host_->Send(new ChromeUtilityMsg_CheckMediaFile(
      kFileDecodeTimeInMS, file_for_transit));
}

SafeAudioVideoChecker::~SafeAudioVideoChecker() {}

void SafeAudioVideoChecker::OnCheckingFinished(bool valid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (state_ != STARTED_STATE)
    return;
  state_ = FINISHED_STATE;

  callback_.Run(valid ? base::File::FILE_OK :
                        base::File::FILE_ERROR_SECURITY);
}

void SafeAudioVideoChecker::OnProcessCrashed(int exit_code) {
  OnCheckingFinished(false);
}

bool SafeAudioVideoChecker::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SafeAudioVideoChecker, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_CheckMediaFile_Finished,
        OnCheckingFinished)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}
