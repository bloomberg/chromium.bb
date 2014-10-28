// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/image_source.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_loader.h"
#include "chrome/common/url_constants.h"
#include "components/user_manager/user_image/user_image.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"

using content::BrowserThread;

namespace chromeos {
namespace {

const char* kWhitelistedFiles[] = {
    "fcc/label.png"
};

}  // namespace

ImageSource::ImageSource() : weak_factory_(this) {
  base::SequencedWorkerPool* blocking_pool =
      BrowserThread::GetBlockingPool();
  task_runner_ = blocking_pool->GetSequencedTaskRunnerWithShutdownBehavior(
      blocking_pool->GetSequenceToken(),
      base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
}

ImageSource::~ImageSource() {
}

std::string ImageSource::GetSource() const {
  return chrome::kChromeOSAssetHost;
}

void ImageSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  if (!IsWhitelisted(path)) {
    callback.Run(NULL);
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ImageSource::StartOnFileThread,
                 weak_factory_.GetWeakPtr(),
                 path,
                 callback));
}

std::string ImageSource::GetMimeType(const std::string& path) const {
  std::string mime_type;
  std::string ext = base::FilePath(path).Extension();
  if (!ext.empty())
    net::GetWellKnownMimeTypeFromExtension(ext.substr(1), &mime_type);
  return mime_type;
}

void ImageSource::StartOnFileThread(
    const std::string& path,
    const content::URLDataSource::GotDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::FilePath file_path(chrome::kChromeOSAssetPath + path);
  if (!base::PathExists(file_path)) {
    callback.Run(NULL);
    return;
  }

  image_loader_ = new UserImageLoader(ImageDecoder::DEFAULT_CODEC,
                                      task_runner_);
  image_loader_->Start(file_path.value(),
                       0,
                       base::Bind(&ImageSource::ImageLoaded,
                                  weak_factory_.GetWeakPtr(),
                                  callback));
}

void ImageSource::ImageLoaded(
    const content::URLDataSource::GotDataCallback& callback,
    const user_manager::UserImage& user_image) const {
  if (user_image.has_raw_image())
    callback.Run(new base::RefCountedBytes(user_image.raw_image()));
  else
    callback.Run(NULL);
}

bool ImageSource::IsWhitelisted(const std::string& path) const {
  const char** end = kWhitelistedFiles + arraysize(kWhitelistedFiles);
  return std::find(kWhitelistedFiles, end, path) != end;
}

}  // namespace chromeos
