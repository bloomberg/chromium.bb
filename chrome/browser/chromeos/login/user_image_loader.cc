// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_loader.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skbitmap_operations.h"

using content::BrowserThread;

namespace chromeos {

UserImageLoader::ImageInfo::ImageInfo(int size,
                                      const LoadedCallback& loaded_cb)
    : size(size),
      loaded_cb(loaded_cb) {
}

UserImageLoader::ImageInfo::~ImageInfo() {
}

UserImageLoader::UserImageLoader(ImageDecoder::ImageCodec image_codec)
    : target_message_loop_(NULL),
      image_codec_(image_codec) {
}

UserImageLoader::~UserImageLoader() {
}

void UserImageLoader::Start(const std::string& filepath,
                            int size,
                            const LoadedCallback& loaded_cb) {
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  SequenceToken sequence_token = pool->GetSequenceToken();
  Start(filepath, size, sequence_token, loaded_cb);
}

void UserImageLoader::Start(const std::string& filepath,
                            int size,
                            const SequenceToken& token,
                            const LoadedCallback& loaded_cb) {
  target_message_loop_ = MessageLoop::current();

  ImageInfo image_info(size, loaded_cb);
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  scoped_refptr<base::SequencedTaskRunner> task_runner = pool->
      GetSequencedTaskRunnerWithShutdownBehavior(token,
          base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(&UserImageLoader::LoadImage, this, filepath, image_info,
                 task_runner));
}

void UserImageLoader::LoadImage(
    const std::string& filepath,
    const ImageInfo& image_info,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK(task_runner->RunsTasksOnCurrentThread());

  std::string image_data;
  file_util::ReadFileToString(base::FilePath(filepath), &image_data);

  scoped_refptr<ImageDecoder> image_decoder =
      new ImageDecoder(this, image_data, image_codec_);
  {
    base::AutoLock lock(lock_);
    image_info_map_.insert(std::make_pair(image_decoder.get(), image_info));
  }
  image_decoder->Start(task_runner);
}

void UserImageLoader::OnImageDecoded(const ImageDecoder* decoder,
                                     const SkBitmap& decoded_image) {
  ImageInfoMap::iterator info_it;
  scoped_ptr<ImageInfo> image_info;
  {
    base::AutoLock lock(lock_);
    info_it = image_info_map_.find(decoder);
    if (info_it == image_info_map_.end()) {
      NOTREACHED();
      return;
    }
    image_info.reset(
        new ImageInfo(info_it->second.size, info_it->second.loaded_cb));
    image_info_map_.erase(info_it);
  }

  SkBitmap final_image = decoded_image;

  if (image_info->size > 0) {
    // Auto crop the image, taking the largest square in the center.
    int size = std::min(decoded_image.width(), decoded_image.height());
    int x = (decoded_image.width() - size) / 2;
    int y = (decoded_image.height() - size) / 2;
    SkBitmap cropped_image =
        SkBitmapOperations::CreateTiledBitmap(decoded_image, x, y, size, size);
    if (size > image_info->size) {
      // Also downsize the image to save space and memory.
      final_image =
          skia::ImageOperations::Resize(cropped_image,
                                        skia::ImageOperations::RESIZE_LANCZOS3,
                                        image_info->size,
                                        image_info->size);
    } else {
      final_image = cropped_image;
    }
  }
  // Make the SkBitmap immutable as we won't modify it. This is important
  // because otherwise it gets duplicated during painting, wasting memory.
  final_image.setImmutable();
  gfx::ImageSkia final_image_skia =
      gfx::ImageSkia::CreateFrom1xBitmap(final_image);
  final_image_skia.MakeThreadSafe();
  UserImage user_image(final_image_skia, decoder->get_image_data());
  if (image_codec_ == ImageDecoder::ROBUST_JPEG_CODEC)
    user_image.MarkAsSafe();
  target_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(image_info->loaded_cb, user_image));
}

void UserImageLoader::OnDecodeImageFailed(const ImageDecoder* decoder) {
  ImageInfoMap::iterator info_it;
  scoped_ptr<ImageInfo> image_info;
  {
    base::AutoLock lock(lock_);
    info_it = image_info_map_.find(decoder);
    if (info_it == image_info_map_.end()) {
      NOTREACHED();
      return;
    }
    image_info.reset(
        new ImageInfo(info_it->second.size, info_it->second.loaded_cb));
    image_info_map_.erase(decoder);
  }

  target_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(image_info->loaded_cb, UserImage()));
}

}  // namespace chromeos
