// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/image_loading_tracker.h"

#include "app/gfx/favicon_size.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/extensions/extension_resource.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/image_decoder.h"

////////////////////////////////////////////////////////////////////////////////
// ImageLoadingTracker::LoadImageTask

// The LoadImageTask is for asynchronously loading the image on the file thread.
// If the image is successfully loaded and decoded it will report back on the
// |callback_loop| to let the caller know the image is done loading.
class ImageLoadingTracker::LoadImageTask : public Task {
 public:
  // Constructor for the LoadImageTask class. |tracker| is the object that
  // we use to communicate back to the entity that wants the image after we
  // decode it. |path| is the path to load the image from. |index| is an
  // identifier for the image that we pass back to the caller.
  // |max_size| is the maximum size for the loaded image. If the image is
  // larger, it will be resized to fit. It is optional.
  LoadImageTask(ImageLoadingTracker* tracker,
                const ExtensionResource& resource,
                size_t index,
                gfx::Size* max_size)
    : callback_loop_(MessageLoop::current()),
      tracker_(tracker),
      resource_(resource),
      index_(index){
    if (max_size)
      max_size_.reset(new gfx::Size(*max_size));
  }

  void ReportBack(SkBitmap* image) {
    DCHECK(image);
    callback_loop_->PostTask(FROM_HERE, NewRunnableMethod(tracker_,
        &ImageLoadingTracker::OnImageLoaded,
        image,
        index_));
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
    webkit_glue::ImageDecoder decoder(gfx::Size(kFavIconSize, kFavIconSize));
    scoped_ptr<SkBitmap> decoded(new SkBitmap());
    *decoded = decoder.Decode(data, file_contents.length());
    if (decoded->empty()) {
      ReportBack(NULL);
      return;  // Unable to decode.
    }

    if (max_size_.get()) {
      if (decoded->width() > max_size_->width() ||
          decoded->height() > max_size_->height()) {
        // The bitmap is not the correct size, re-sample.
        int new_width = decoded->width();
        int new_height = decoded->height();
        // Calculate what dimensions to use within the constraints (16x16 max).
        calc_favicon_target_size(&new_width, &new_height);
        *decoded = skia::ImageOperations::Resize(
            *decoded, skia::ImageOperations::RESIZE_LANCZOS3,
            new_width, new_height);
      }
    }

    ReportBack(decoded.release());
  }

 private:
  // The message loop that we need to call back on to report that we are done.
  MessageLoop* callback_loop_;

  // The object that is waiting for us to respond back.
  ImageLoadingTracker* tracker_;

  // The image resource to load asynchronously.
  ExtensionResource resource_;

  // The index of the icon being loaded.
  size_t index_;

  // The max size for the image. If the image is larger than this, it will be
  // scaled down.
  scoped_ptr<gfx::Size> max_size_;
};

////////////////////////////////////////////////////////////////////////////////
// ImageLoadingTracker

void ImageLoadingTracker::PostLoadImageTask(const ExtensionResource& resource,
                                            gfx::Size* max_size) {
  MessageLoop* file_loop = g_browser_process->file_thread()->message_loop();
  file_loop->PostTask(FROM_HERE, new LoadImageTask(this, resource,
                                                   posted_count_++, max_size));
}

void ImageLoadingTracker::OnImageLoaded(SkBitmap* image, size_t index) {
  if (image == NULL) {
    NOTREACHED() << "Image failed to decode.";
    image = new SkBitmap();
  }
  if (observer_)
    observer_->OnImageLoaded(image, index);
  delete image;
  if (--image_count_ == 0)
    Release();  // We are no longer needed.
}
