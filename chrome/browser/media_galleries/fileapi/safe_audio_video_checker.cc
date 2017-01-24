// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/safe_audio_video_checker.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

SafeAudioVideoChecker::SafeAudioVideoChecker(
    base::File file,
    const storage::CopyOrMoveFileValidator::ResultCallback& callback)
    : file_(std::move(file)), callback_(callback) {
  DCHECK(!callback_.is_null());
}

void SafeAudioVideoChecker::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!file_.IsValid()) {
    callback_.Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  DCHECK(!utility_process_mojo_client_);

  const base::string16 utility_process_name =
      l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_MEDIA_FILE_CHECKER_NAME);

  utility_process_mojo_client_.reset(
      new content::UtilityProcessMojoClient<extensions::mojom::MediaParser>(
          utility_process_name));
  utility_process_mojo_client_->set_error_callback(
      base::Bind(&SafeAudioVideoChecker::CheckMediaFileDone, this, false));

  utility_process_mojo_client_->Start();  // Start the utility process.

  constexpr auto kFileDecodeTime = base::TimeDelta::FromMilliseconds(250);

  utility_process_mojo_client_->service()->CheckMediaFile(
      kFileDecodeTime, std::move(file_),
      base::Bind(&SafeAudioVideoChecker::CheckMediaFileDone, this));
}

void SafeAudioVideoChecker::CheckMediaFileDone(bool valid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  utility_process_mojo_client_.reset();  // Terminate the utility process.
  callback_.Run(valid ? base::File::FILE_OK : base::File::FILE_ERROR_SECURITY);
}

SafeAudioVideoChecker::~SafeAudioVideoChecker() = default;
