// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_IMAGE_LOADING_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_IMAGE_LOADING_TRACKER_H_
#pragma once

#include <map>

#include "base/ref_counted.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class Extension;
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
//   tracker_.LoadImage(extension, resource, max_size, false);
// ... and wait for OnImageLoaded to be called back on you with a pointer to the
// SkBitmap loaded.
// NOTE: if the image is available already (or the resource is not valid), the
// Observer is notified immediately from the call to LoadImage. In other words,
// by the time LoadImage returns the observer has been notified.
//
class ImageLoadingTracker : public NotificationObserver {
 public:
  enum CacheParam {
    CACHE,
    DONT_CACHE
  };

  class Observer {
   public:
    virtual ~Observer();

    // Will be called when the image with the given index has loaded.
    // The |image| is owned by the tracker, so the observer should make a copy
    // if they need to access it after this call. |image| can be null if valid
    // image was not found or it failed to decode. |resource| is the
    // ExtensionResource where the |image| came from and the |index| represents
    // the index of the image just loaded (starts at 0 and increments every
    // time LoadImage is called).
    virtual void OnImageLoaded(SkBitmap* image, ExtensionResource resource,
                               int index) = 0;
  };

  explicit ImageLoadingTracker(Observer* observer);
  ~ImageLoadingTracker();

  // Specify image resource to load. If the loaded image is larger than
  // |max_size| it will be resized to those dimensions. IMPORTANT NOTE: this
  // function may call back your observer synchronously (ie before it returns)
  // if the image was found in the cache.
  void LoadImage(const Extension* extension,
                 const ExtensionResource& resource,
                 const gfx::Size& max_size,
                 CacheParam cache);

 private:
  typedef std::map<int, const Extension*> LoadMap;

  class ImageLoader;

  // When an image has finished loaded and been resized on the file thread, it
  // is posted back to this method on the original thread.  This method then
  // calls the observer's OnImageLoaded and deletes the ImageLoadingTracker if
  // it was the last image in the list. The |original_size| should be the size
  // of the image before any resizing was done.
  // |image| may be null if the file failed to decode.
  void OnImageLoaded(SkBitmap* image, const ExtensionResource& resource,
                     const gfx::Size& original_size, int id);

  // NotificationObserver method. If an extension is uninstalled while we're
  // waiting for the image we remove the entry from load_map_.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // The view that is waiting for the image to load.
  Observer* observer_;

  // ID to use for next image requested. This is an ever increasing integer.
  int next_id_;

  // The object responsible for loading the image on the File thread.
  scoped_refptr<ImageLoader> loader_;

  // If LoadImage is told to cache the result an entry is added here. The
  // integer identifies the id assigned to the request. If the extension is
  // deleted while fetching the image the entry is removed from the map.
  LoadMap load_map_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ImageLoadingTracker);
};

#endif  // CHROME_BROWSER_EXTENSIONS_IMAGE_LOADING_TRACKER_H_
