// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_IMAGE_LOADING_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_IMAGE_LOADING_TRACKER_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/layout.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/size.h"

class SkBitmap;

namespace extensions {
class AppShortcutManager;
class Extension;
class IconImage;
class TabHelper;
}

namespace gfx {
class Image;
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
// ImageSkia loaded.
// NOTE: if the image is available already (or the resource is not valid), the
// Observer is notified immediately from the call to LoadImage. In other words,
// by the time LoadImage returns the observer has been notified.
//
// NOTE: This class is deprecated! New code should be using
// extensions::ImageLoader or extensions::IconImage instead.
class ImageLoadingTracker : public content::NotificationObserver {
 public:
  enum CacheParam {
    CACHE,
    DONT_CACHE
  };

  class Observer {
   public:
    // Will be called when the image with the given index has loaded.
    // |image| can be empty if a valid image was not found or it failed to
    // decode. |extension_id| is the ID of the extension the images are loaded
    // from.  |index| represents the index of the image just loaded (starts at 0
    // and increments every time LoadImage is called).
    virtual void OnImageLoaded(const gfx::Image& image,
                               const std::string& extension_id,
                               int index) = 0;

   protected:
    virtual ~Observer();
  };

  // Information about a singe image representation to load from an extension
  // resource.
  struct ImageRepresentation {
    // Enum values to indicate whether to resize loaded bitmap when it is larger
    // than |desired_size| or always resize it.
    enum ResizeCondition {
      RESIZE_WHEN_LARGER,
      ALWAYS_RESIZE,
    };

    ImageRepresentation(const ExtensionResource& resource,
                        ResizeCondition resize_method,
                        const gfx::Size& desired_size,
                        ui::ScaleFactor scale_factor);
    ~ImageRepresentation();

    // Extension resource to load.
    ExtensionResource resource;

    ResizeCondition resize_method;

    // When |resize_method| is ALWAYS_RESIZE or when the loaded image is larger
    // than |desired_size| it will be resized to these dimensions.
    gfx::Size desired_size;

    // |scale_factor| is used to construct the loaded gfx::ImageSkia.
    ui::ScaleFactor scale_factor;
  };

  virtual ~ImageLoadingTracker();

  // Specify image resource to load. If the loaded image is larger than
  // |max_size| it will be resized to those dimensions. IMPORTANT NOTE: this
  // function may call back your observer synchronously (ie before it returns)
  // if the image was found in the cache.
  // Note this method loads a raw bitmap from the resource. All sizes given are
  // assumed to be in pixels.
  void LoadImage(const extensions::Extension* extension,
                 const ExtensionResource& resource,
                 const gfx::Size& max_size,
                 CacheParam cache);

  // Same as LoadImage() above except it loads multiple images from the same
  // extension. This is used to load multiple resolutions of the same image
  // type.
  void LoadImages(const extensions::Extension* extension,
                  const std::vector<ImageRepresentation>& info_list,
                  CacheParam cache);

  // Returns the ID used for the next image that is loaded. That is, the return
  // value from this method corresponds to the int that is passed to
  // OnImageLoaded() the next time LoadImage() is invoked.
  int next_id() const { return next_id_; }

 private:
  // This class is deprecated. This just lists all currently remaining
  // usage of this class, so once this list is empty this class can and
  // should be removed.
  friend class CreateChromeApplicationShortcutsDialogGtk;
  friend class CreateChromeApplicationShortcutView;
  friend class ExtensionIconManager;
  friend class ExtensionInfoBar;
  friend class ExtensionInfoBarGtk;
  friend class InfobarBridge;
  friend class Panel;
  friend class ShellWindow;
  friend class extensions::IconImage;
  friend class extensions::TabHelper;
  FRIEND_TEST_ALL_PREFIXES(ImageLoadingTrackerTest, Cache);
  FRIEND_TEST_ALL_PREFIXES(ImageLoadingTrackerTest,
                           DeleteExtensionWhileWaitingForCache);
  FRIEND_TEST_ALL_PREFIXES(ImageLoadingTrackerTest, MultipleImages);

  explicit ImageLoadingTracker(Observer* observer);

  // Information for pending resource load operation for one or more image
  // representations.
  struct PendingLoadInfo {
    PendingLoadInfo();
    ~PendingLoadInfo();

    const extensions::Extension* extension;
    // This is cached separate from |extension| in case the extension is
    // unloaded.
    std::string extension_id;
    CacheParam cache;
    size_t pending_count;
    gfx::ImageSkia image_skia;
  };

  // Maps an integer identifying a load request to a PendingLoadInfo.
  typedef std::map<int, PendingLoadInfo> LoadMap;

  class ImageLoader;

  // Called on the calling thread when the bitmap finishes loading.
  // |bitmap| may be null if the image file failed to decode.
  void OnBitmapLoaded(const SkBitmap* bitmap,
                      const ImageRepresentation& image_info,
                      const gfx::Size& original_size,
                      int id,
                      bool should_cache);

  // content::NotificationObserver method. If an extension is uninstalled while
  // we're waiting for the image we remove the entry from load_map_.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // The view that is waiting for the image to load.
  Observer* observer_;

  // ID to use for next image requested. This is an ever increasing integer.
  int next_id_;

  // The object responsible for loading the image on the File thread.
  scoped_refptr<ImageLoader> loader_;

  // Information for each LoadImage request is cached here. The integer
  // identifies the id assigned to the request.
  LoadMap load_map_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ImageLoadingTracker);
};

#endif  // CHROME_BROWSER_EXTENSIONS_IMAGE_LOADING_TRACKER_H_
