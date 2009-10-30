// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/image_loading_tracker.h"

#include "base/file_util.h"
#include "base/gfx/size.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/extensions/extension_resource.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/image_decoder.h"

////////////////////////////////////////////////////////////////////////////////
// ImageLoadingTracker::LoadImageTask

// The LoadImageTask is for asynchronously loading the image on the file thread.
// If the image is successfully loaded and decoded it will report back on the
// calling thread to let the caller know the image is done loading.
class ImageLoadingTracker::LoadImageTask : public Task {
 public:
  // Constructor for the LoadImageTask class. |tracker| is the object that
  // we use to communicate back to the entity that wants the image after we
  // decode it. |path| is the path to load the image from. |max_size| is the
  // maximum size for the loaded image. It will be resized to fit this if
  // larger. |index| is an identifier for the image that we pass back to the
  // caller.
  LoadImageTask(ImageLoadingTracker* tracker,
                const ExtensionResource& resource,
                const gfx::Size& max_size,
                size_t index)
    : tracker_(tracker),
      resource_(resource),
      max_size_(max_size),
      index_(index) {
    CHECK(ChromeThread::GetCurrentThreadIdentifier(&callback_thread_id_));
  }

  void ReportBack(SkBitmap* image) {
    ChromeThread::PostTask(
        callback_thread_id_, FROM_HERE,
        NewRunnableMethod(
            tracker_, &ImageLoadingTracker::OnImageLoaded, image, index_));
  }

  virtual void Run() {
    // Read the file from disk.
    std::string file_contents;
    FilePath path = resource_.GetFilePath();
    if (path.empty() || !file_util::ReadFileToString(path, &file_contents)) {
      ReportBack(NULL);
      return;
    }

    // Decode the image using WebKit's image decoder.
    const unsigned char* data =
        reinterpret_cast<const unsigned char*>(file_contents.data());
    webkit_glue::ImageDecoder decoder;
    scoped_ptr<SkBitmap> decoded(new SkBitmap());
    *decoded = decoder.Decode(data, file_contents.length());
    if (decoded->empty()) {
      ReportBack(NULL);
      return;  // Unable to decode.
    }

    if (decoded->width() > max_size_.width() ||
        decoded->height() > max_size_.height()) {
      // The bitmap is too big, re-sample.
      *decoded = skia::ImageOperations::Resize(
          *decoded, skia::ImageOperations::RESIZE_LANCZOS3,
          max_size_.width(), max_size_.height());
    }

    ReportBack(decoded.release());
  }

 private:
  // The thread that we need to call back on to report that we are done.
  ChromeThread::ID callback_thread_id_;

  // The object that is waiting for us to respond back.
  ImageLoadingTracker* tracker_;

  // The image resource to load asynchronously.
  ExtensionResource resource_;

  // The max size for the loaded image.
  gfx::Size max_size_;

  // The index of the icon being loaded.
  size_t index_;
};

////////////////////////////////////////////////////////////////////////////////
// ImageLoadingTracker

void ImageLoadingTracker::PostLoadImageTask(const ExtensionResource& resource,
                                            const gfx::Size& max_size) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      new LoadImageTask(this, resource, max_size, posted_count_++));
}

void ImageLoadingTracker::OnImageLoaded(SkBitmap* image, size_t index) {
  if (observer_)
    observer_->OnImageLoaded(image, index);

  if (image)
    delete image;

  if (--image_count_ == 0)
    Release();  // We are no longer needed.
}
