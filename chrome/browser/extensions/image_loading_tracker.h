// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_IMAGE_LOADING_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_IMAGE_LOADING_TRACKER_H_

#include "base/ref_counted.h"
#include "chrome/common/extensions/extension.h"

class ExtensionResource;
class SkBitmap;

namespace gfx {
  class Size;
}

// The views need to load their icons asynchronously but might be deleted before
// the images have loaded. This class encapsulates a loader class that stays
// alive while the request is in progress (manages its own lifetime) and keeps
// track of whether the view still cares about the icon loading.
//
// To use this class, have your class derive from ImageLoadingTracker::Observer,
// and add a member variable ImageLoadingTracker tracker_. Then override
// Observer::OnImageLoaded and call:
//   tracker_.LoadImage(resource, max_size);
// ... and wait for OnImageLoaded to be called back on you with a pointer to the
// SkBitmap loaded.
//
class ImageLoadingTracker {
 public:
  class Observer {
   public:
    // Will be called when the image with the given index has loaded.
    // The |image| is owned by the tracker, so the observer should make a copy
    // if they need to access it after this call. |image| can be null if valid
    // image was not found or it failed to decode. |resource| is the
    // ExtensionResource where the |image| came from and the |index| represents
    // the index of the image just loaded (starts at 0 and increments every
    // time this function is called).
    virtual void OnImageLoaded(SkBitmap* image, ExtensionResource resource,
                               int index) = 0;
  };

  explicit ImageLoadingTracker(Observer* observer);
  ~ImageLoadingTracker();

  // Specify image resource to load. If the loaded image is larger than
  // |max_size| it will be resized to those dimensions.
  void LoadImage(const ExtensionResource& resource,
                 gfx::Size max_size);

 private:
  class ImageLoader;

  // When an image has finished loaded and been resized on the file thread, it
  // is posted back to this method on the original thread.  This method then
  // calls the observer's OnImageLoaded and deletes the ImageLoadingTracker if
  // it was the last image in the list.
  // |image| may be null if the file failed to decode.
  void OnImageLoaded(SkBitmap* image, const ExtensionResource& resource);

  // The view that is waiting for the image to load.
  Observer* observer_;

  // The number of times we've reported back.
  int responses_;

  // The object responsible for loading the image on the File thread.
  scoped_refptr<ImageLoader> loader_;

  DISALLOW_COPY_AND_ASSIGN(ImageLoadingTracker);
};

#endif  // CHROME_BROWSER_EXTENSIONS_IMAGE_LOADING_TRACKER_H_
