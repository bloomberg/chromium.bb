// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/image_source.h"

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_loader.h"
#include "chrome/common/url_constants.h"
#include "components/user_manager/user_image/user_image.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"

using content::BrowserThread;

namespace chromeos {
namespace {

const char* kWhitelistedDirectories[] = {
  "regulatory_labels"
};

// Callback for user_manager::UserImageLoader.
void ImageLoaded(
    const content::URLDataSource::GotDataCallback& got_data_callback,
    std::unique_ptr<user_manager::UserImage> user_image) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (user_image->has_image_bytes())
    got_data_callback.Run(user_image->image_bytes());
  else
    got_data_callback.Run(nullptr);
}

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
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& got_data_callback) {
  if (!IsWhitelisted(path)) {
    got_data_callback.Run(nullptr);
    return;
  }

  const base::FilePath asset_dir(FILE_PATH_LITERAL(chrome::kChromeOSAssetPath));
  const base::FilePath image_path = asset_dir.AppendASCII(path);
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&base::PathExists, image_path),
      base::Bind(&ImageSource::StartDataRequestAfterPathExists,
                 weak_factory_.GetWeakPtr(), image_path, got_data_callback));
}

void ImageSource::StartDataRequestAfterPathExists(
    const base::FilePath& image_path,
    const content::URLDataSource::GotDataCallback& got_data_callback,
    bool path_exists) {
  if (path_exists) {
    user_image_loader::StartWithFilePath(
        task_runner_,
        image_path,
        ImageDecoder::DEFAULT_CODEC,
        0,  // Do not crop.
        base::Bind(&ImageLoaded, got_data_callback));
  } else {
    got_data_callback.Run(nullptr);
  }
}

std::string ImageSource::GetMimeType(const std::string& path) const {
  std::string mime_type;
  std::string ext = base::FilePath(path).Extension();
  if (!ext.empty())
    net::GetWellKnownMimeTypeFromExtension(ext.substr(1), &mime_type);
  return mime_type;
}

bool ImageSource::IsWhitelisted(const std::string& path) const {
  base::FilePath file_path(path);
  if (file_path.ReferencesParent())
    return false;

  // Check if the path starts with a whitelisted directory.
  std::vector<std::string> components;
  file_path.GetComponents(&components);
  if (components.empty())
    return false;

  for (size_t i = 0; i < arraysize(kWhitelistedDirectories); i++) {
    if (components[0] == kWhitelistedDirectories[i])
      return true;
  }
  return false;
}

}  // namespace chromeos
