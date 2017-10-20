// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/supported_image_type_validator.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/image_decoder.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Arbitrary limit to sanity check the file size.
const int kMaxImageFileSize = 50*1014*1024;

std::unique_ptr<std::string> ReadOnFileThread(const base::FilePath& path) {
  base::AssertBlockingAllowed();
  std::unique_ptr<std::string> result;

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return result;

  base::File::Info file_info;
  if (!file.GetInfo(&file_info) ||
      file_info.size > kMaxImageFileSize) {
    return result;
  }

  result.reset(new std::string);
  result->resize(file_info.size);
  if (file.Read(0, base::string_as_array(result.get()), file_info.size) !=
      file_info.size) {
    result.reset();
  }

  return result;
}

class ImageDecoderDelegateAdapter : public ImageDecoder::ImageRequest {
 public:
  ImageDecoderDelegateAdapter(
      std::unique_ptr<std::string> data,
      const storage::CopyOrMoveFileValidator::ResultCallback& callback)
      : data_(std::move(data)), callback_(callback) {
    DCHECK(data_);
  }

  const std::string& data() {
    return *data_;
  }

  // ImageDecoder::ImageRequest methods.
  void OnImageDecoded(const SkBitmap& /*decoded_image*/) override {
    callback_.Run(base::File::FILE_OK);
    delete this;
  }

  void OnDecodeImageFailed() override {
    callback_.Run(base::File::FILE_ERROR_SECURITY);
    delete this;
  }

 private:
  std::unique_ptr<std::string> data_;
  storage::CopyOrMoveFileValidator::ResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecoderDelegateAdapter);
};

}  // namespace

SupportedImageTypeValidator::~SupportedImageTypeValidator() {}

// static
bool SupportedImageTypeValidator::SupportsFileType(const base::FilePath& path) {
  base::FilePath::StringType extension = path.Extension();
  return extension == FILE_PATH_LITERAL(".bmp") ||
         extension == FILE_PATH_LITERAL(".gif") ||
         extension == FILE_PATH_LITERAL(".jfif") ||
         extension == FILE_PATH_LITERAL(".jpeg") ||
         extension == FILE_PATH_LITERAL(".jpg") ||
         extension == FILE_PATH_LITERAL(".pjp") ||
         extension == FILE_PATH_LITERAL(".pjpeg") ||
         extension == FILE_PATH_LITERAL(".png") ||
         extension == FILE_PATH_LITERAL(".webp");
}

void SupportedImageTypeValidator::StartPreWriteValidation(
    const ResultCallback& result_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(callback_.is_null());
  callback_ = result_callback;

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&ReadOnFileThread, path_),
      base::Bind(&SupportedImageTypeValidator::OnFileOpen,
                 weak_factory_.GetWeakPtr()));
}

SupportedImageTypeValidator::SupportedImageTypeValidator(
    const base::FilePath& path)
    : path_(path),
      weak_factory_(this) {
}

void SupportedImageTypeValidator::OnFileOpen(
    std::unique_ptr<std::string> data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!data.get()) {
    callback_.Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  // |adapter| will delete itself after a completion message is received.
  ImageDecoderDelegateAdapter* adapter =
      new ImageDecoderDelegateAdapter(std::move(data), callback_);
  ImageDecoder::Start(adapter, adapter->data());
}
