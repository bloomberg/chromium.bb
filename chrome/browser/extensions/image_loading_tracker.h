// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_IMAGE_LOADING_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_IMAGE_LOADING_TRACKER_H_

#include "base/ref_counted.h"

class ExtensionResource;
class SkBitmap;

namespace gfx {
class Size;
}

// The views need to load their icons asynchronously but might be deleted before
// the images have loaded. This class stays alive while the request is in
// progress (manages its own lifetime) and keeps track of whether the view still
// cares about the icon loading.
// Consider abstracting out a FilePathProvider (ExtensionResource) and moving
// back to chrome/browser/ if other subsystems want to use it.
class ImageLoadingTracker
  : public base::RefCountedThreadSafe<ImageLoadingTracker> {
 public:
  class Observer {
   public:
    // Will be called when the image with the given index has loaded.
    // The |image| is owned by the tracker, so the observer should make a copy
    // if they need to access it after this call.
    virtual void OnImageLoaded(SkBitmap* image, size_t index) = 0;
  };

  ImageLoadingTracker(Observer* observer, size_t image_count)
    : observer_(observer), image_count_(image_count), posted_count_(0) {
    AddRef();  // We hold on to a reference to ourself to make sure we don't
               // get deleted until we get a response from image loading (see
               // ImageLoadingDone).
  }
  ~ImageLoadingTracker() {}

  // If there are remaining images to be loaded, the observing object should
  // call this method on its destruction, so that the tracker will not attempt
  // to make any more callbacks to it.
  void StopTrackingImageLoad() {
    observer_ = NULL;
  }

  // Specify image resource to load.  This method must be called a number of
  // times equal to the |image_count| arugment to the constructor.  Calling it
  // any more or less than that is an error. If the loaded image is larger than
  // |max_size| it will be resized to those dimensions.
  void PostLoadImageTask(const ExtensionResource& resource,
                         const gfx::Size& max_size);

 private:
  class LoadImageTask;

  // When an image has finished loaded and scaled on the file thread, it is
  // posted back to this method on the original thread.  This method then calls
  // the observer's OnImageLoaded and deletes the ImageLoadingTracker if it was
  // the last image in the list.
  // |image| may be null if the file failed to decode.
  void OnImageLoaded(SkBitmap* image, size_t index);

  // The view that is waiting for the image to load.
  Observer* observer_;

  // The number of images this ImageTracker should keep track of.  This is
  // decremented as each image finishes loading, and the tracker will delete
  // itself when it reaches zero.
  size_t image_count_;

  // The number of tasks that have been posted so far.
  size_t posted_count_;

  DISALLOW_COPY_AND_ASSIGN(ImageLoadingTracker);
};

#endif  // CHROME_BROWSER_EXTENSIONS_IMAGE_LOADING_TRACKER_H_
