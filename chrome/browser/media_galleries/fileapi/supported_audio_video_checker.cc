// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/supported_audio_video_checker.h"

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/media_galleries/fileapi/safe_audio_video_checker.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"

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
          net::IsSupportedMimeType(mime_type)) {
        audio_video_extensions_.insert(
            base::FilePath::kExtensionSeparator + extensions[i]);
      }
    }
  };

  bool HasSupportedAudioVideoExtension(const base::FilePath& file) {
    return ContainsKey(audio_video_extensions_, file.Extension());
  }

 private:
  std::set<base::FilePath::StringType> audio_video_extensions_;

  DISALLOW_COPY_AND_ASSIGN(SupportedAudioVideoExtensions);
};

base::LazyInstance<SupportedAudioVideoExtensions> g_audio_video_extensions =
    LAZY_INSTANCE_INITIALIZER;

base::PlatformFile OpenOnFileThread(const base::FilePath& path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  return base::CreatePlatformFile(
      path, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL /*created*/, NULL /*error_code*/);
}

}  // namespace

SupportedAudioVideoChecker::~SupportedAudioVideoChecker() {}

// static
bool SupportedAudioVideoChecker::SupportsFileType(const base::FilePath& path) {
  return g_audio_video_extensions.Get().HasSupportedAudioVideoExtension(path);
}

void SupportedAudioVideoChecker::StartPreWriteValidation(
    const fileapi::CopyOrMoveFileValidator::ResultCallback& result_callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(callback_.is_null());
  callback_ = result_callback;

  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&OpenOnFileThread, path_),
      base::Bind(&SupportedAudioVideoChecker::OnFileOpen,
                 weak_factory_.GetWeakPtr()));
}

SupportedAudioVideoChecker::SupportedAudioVideoChecker(
    const base::FilePath& path)
    : path_(path),
      weak_factory_(this) {
}

void SupportedAudioVideoChecker::OnFileOpen(const base::PlatformFile& file) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (file == base::kInvalidPlatformFileValue) {
    callback_.Run(base::PLATFORM_FILE_ERROR_SECURITY);
    return;
  }

  safe_checker_ = new SafeAudioVideoChecker(file, callback_);
  safe_checker_->Start();
}
