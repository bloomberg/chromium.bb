// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_IMAGE_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_IMAGE_LOADER_H_

#include <map>
#include <set>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "extensions/common/extension_resource.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/gfx/size.h"

class Profile;

namespace gfx {
class Image;
}

namespace extensions {

class Extension;

// This class is responsible for asynchronously loading extension images and
// calling a callback when an image is loaded.
// The views need to load their icons asynchronously might be deleted before
// the images have loaded. If you pass your callback using a weak_ptr, this
// will make sure the callback won't be called after the view is deleted.
class ImageLoader : public ProfileKeyedService {
 public:
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
                        ResizeCondition resize_condition,
                        const gfx::Size& desired_size,
                        ui::ScaleFactor scale_factor);
    ~ImageRepresentation();

    // Extension resource to load.
    ExtensionResource resource;

    ResizeCondition resize_condition;

    // When |resize_method| is ALWAYS_RESIZE or when the loaded image is larger
    // than |desired_size| it will be resized to these dimensions.
    gfx::Size desired_size;

    // |scale_factor| is used to construct the loaded gfx::ImageSkia.
    ui::ScaleFactor scale_factor;
  };

  struct LoadResult;

  // Returns the instance for the given profile, or NULL if none. This is
  // a convenience wrapper around ImageLoaderFactory::GetForProfile.
  static ImageLoader* Get(Profile* profile);

  ImageLoader();
  virtual ~ImageLoader();

  // Checks whether image is a component extension resource. Returns false
  // if a given |resource| does not have a corresponding image in bundled
  // resources. Otherwise fills |resource_id|. This doesn't check if the
  // extension the resource is in is actually a component extension.
  static bool IsComponentExtensionResource(
      const base::FilePath& extension_path,
      const base::FilePath& resource_path,
      int* resource_id);

  // Specify image resource to load. If the loaded image is larger than
  // |max_size| it will be resized to those dimensions. IMPORTANT NOTE: this
  // function may call back your callback synchronously (ie before it returns)
  // if the image was found in the cache.
  // Note this method loads a raw bitmap from the resource. All sizes given are
  // assumed to be in pixels.
  void LoadImageAsync(const extensions::Extension* extension,
                      const ExtensionResource& resource,
                      const gfx::Size& max_size,
                      const base::Callback<void(const gfx::Image&)>& callback);

  // Same as LoadImage() above except it loads multiple images from the same
  // extension. This is used to load multiple resolutions of the same image
  // type.
  void LoadImagesAsync(const extensions::Extension* extension,
                       const std::vector<ImageRepresentation>& info_list,
                       const base::Callback<void(const gfx::Image&)>& callback);

 private:
  base::WeakPtrFactory<ImageLoader> weak_ptr_factory_;

  static void LoadImagesOnBlockingPool(
      const std::vector<ImageRepresentation>& info_list,
      const std::vector<SkBitmap>& bitmaps,
      std::vector<LoadResult>* load_result);

  void ReplyBack(
      const std::vector<LoadResult>* load_result,
      const base::Callback<void(const gfx::Image&)>& callback);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_IMAGE_LOADER_H_
