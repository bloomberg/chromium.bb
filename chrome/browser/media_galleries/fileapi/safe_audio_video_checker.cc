// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/safe_audio_video_checker.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/base/l10n/l10n_util.h"

// TODO(noel): remove state_ once this code is all mojo.
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

  utility_process_host_ = content::UtilityProcessHost::Create(
      this, base::ThreadTaskRunnerHandle::Get())->AsWeakPtr();
  utility_process_host_->SetName(l10n_util::GetStringUTF16(
      IDS_UTILITY_PROCESS_MEDIA_FILE_CHECKER_NAME));

  utility_process_host_->Start();

  utility_process_host_->GetRemoteInterfaces()->GetInterface(&interface_);
  interface_.set_connection_error_handler(
      base::Bind(&SafeAudioVideoChecker::OnCheckingFinished, this, false));

  constexpr base::TimeDelta kFileDecodeTime =
      base::TimeDelta::FromMilliseconds(250);
  interface_->CheckMediaFile(
      kFileDecodeTime, std::move(file_),
      base::Bind(&SafeAudioVideoChecker::OnCheckingFinished, this));
}

SafeAudioVideoChecker::~SafeAudioVideoChecker() {}

void SafeAudioVideoChecker::OnCheckingFinished(bool valid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (state_ != STARTED_STATE)
    return;
  state_ = FINISHED_STATE;

  interface_.reset();
  callback_.Run(valid ? base::File::FILE_OK :
                        base::File::FILE_ERROR_SECURITY);
}

// TODO(noel): remove, use the utility process mojo host client.
void SafeAudioVideoChecker::OnProcessCrashed(int exit_code) {
  OnCheckingFinished(false);
}

// TODO(noel): remove, use the utility process mojo host client.
bool SafeAudioVideoChecker::OnMessageReceived(const IPC::Message& message) {
  return false;
}
