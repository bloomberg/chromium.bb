// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/supported_audio_video_checker.h"

#include <stddef.h>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/services/media_gallery_util/public/cpp/safe_audio_video_checker.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

namespace {

class SupportedAudioVideoExtensions {
 public:
  SupportedAudioVideoExtensions() {
    std::vector<base::FilePath::StringType> extensions;
    net::GetExtensionsForMimeType("audio/*", &extensions);
    net::GetExtensionsForMimeType("video/*", &extensions);
    for (size_t i = 0; i < extensions.size(); ++i) {
      std::string mime_type;
      if (net::GetWellKnownMimeTypeFromExtension(extensions[i], &mime_type) &&
          blink::IsSupportedMimeType(mime_type)) {
        audio_video_extensions_.insert(
            base::FilePath::kExtensionSeparator + extensions[i]);
      }
    }
  }

  bool HasSupportedAudioVideoExtension(const base::FilePath& file) {
    return base::Contains(audio_video_extensions_, file.Extension());
  }

 private:
  std::set<base::FilePath::StringType> audio_video_extensions_;

  DISALLOW_COPY_AND_ASSIGN(SupportedAudioVideoExtensions);
};

base::LazyInstance<SupportedAudioVideoExtensions>::DestructorAtExit
    g_audio_video_extensions = LAZY_INSTANCE_INITIALIZER;

base::File OpenBlocking(const base::FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
}

}  // namespace

SupportedAudioVideoChecker::~SupportedAudioVideoChecker() {}

// static
bool SupportedAudioVideoChecker::SupportsFileType(const base::FilePath& path) {
  return g_audio_video_extensions.Get().HasSupportedAudioVideoExtension(path);
}

void SupportedAudioVideoChecker::StartPreWriteValidation(
    storage::CopyOrMoveFileValidator::ResultCallback result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(callback_.is_null());
  callback_ = std::move(result_callback);

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&OpenBlocking, path_),
      base::BindOnce(&SupportedAudioVideoChecker::OnFileOpen,
                     weak_factory_.GetWeakPtr()));
}

SupportedAudioVideoChecker::SupportedAudioVideoChecker(
    const base::FilePath& path)
    : path_(path) {}

void SupportedAudioVideoChecker::OnFileOpen(base::File file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!file.IsValid()) {
    std::move(callback_).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  safe_checker_ = std::make_unique<SafeAudioVideoChecker>(std::move(file),
                                                          std::move(callback_));
  safe_checker_->Start();
}
