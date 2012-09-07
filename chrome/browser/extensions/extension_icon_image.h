// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ICON_IMAGE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ICON_IMAGE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {
class Extension;
}

namespace gfx {
class Size;
}

namespace extensions {

// A class that provides an ImageSkia for UI code to use. It handles extension
// icon resource loading, screen scale factor change etc. UI code that uses
// extension icon should host this class and be its observer. ExtensionIconImage
// should be outlived by the observer. In painting code, UI code paints with the
// ImageSkia provided by this class. If required extension icon resource is not
// already present, this class tries to load it and calls its observer interface
// when the image get updated. Until the resource is loaded, the UI code will be
// provided with blank, transparent image.
// If the requested resource doesn't exist or can't be loaded and a default
// icon was supplied in the constructor, icon image will be updated with the
// default icon's resource.
// The default icon doesn't need to be supplied, but in that case, icon image
// representation will be left blank if the resource loading fails.
// If default icon is supplied, it is assumed that it contains or can
// synchronously create (when |GetRepresentation| is called on it)
// representations for all the scale factors supported by the current platform.
// Note that |IconImage| is not thread safe.
class IconImage : public ImageLoadingTracker::Observer,
                  public content::NotificationObserver {
 public:
  class Observer {
   public:
    // Invoked when a new image rep for an additional scale factor
    // is loaded and added to |image|.
    virtual void OnExtensionIconImageChanged(IconImage* image) = 0;

   protected:
    virtual ~Observer() {}
  };

  IconImage(const Extension* extension,
            const ExtensionIconSet& icon_set,
            int resource_size_in_dip,
            const gfx::ImageSkia& default_icon,
            Observer* observer);
  virtual ~IconImage();

  const gfx::ImageSkia& image_skia() const { return image_skia_; }

 private:
  class Source;

  struct LoadRequest {
    ui::ScaleFactor scale_factor;
    bool is_async;
  };

  typedef std::map<int, LoadRequest> LoadMap;

  // Loads an image representation for the scale factor.
  // If the representation gets loaded synchronously, it is returned by this
  // method.
  // If representation loading is asynchronous, an empty image
  // representation is returned. When the representation gets loaded the
  // observer's |OnExtensionIconImageLoaded| will be called.
  gfx::ImageSkiaRep LoadImageForScaleFactor(ui::ScaleFactor scale_factor);

  // ImageLoadingTracker::Observer overrides:
  virtual void OnImageLoaded(const gfx::Image& image,
                             const std::string& extension_id,
                             int index) OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  const Extension* extension_;
  const ExtensionIconSet& icon_set_;
  const int resource_size_in_dip_;

  Observer* observer_;

  Source* source_;  // Owned by ImageSkia storage.
  gfx::ImageSkia image_skia_;
  // The icon with whose representation |image_skia_| should be updated if
  // its own representation load fails.
  gfx::ImageSkia default_icon_;

  ImageLoadingTracker tracker_;
  content::NotificationRegistrar registrar_;

  LoadMap load_map_;

  DISALLOW_COPY_AND_ASSIGN(IconImage);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ICON_IMAGE_H_
