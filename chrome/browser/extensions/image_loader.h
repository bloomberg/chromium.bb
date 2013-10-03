// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_IMAGE_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_IMAGE_LOADER_H_

#include <set>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "extensions/common/extension_resource.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/gfx/size.h"

class GURL;
class Profile;

namespace base {
class RefCountedMemory;
}

namespace chrome {
struct FaviconBitmapResult;
}

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
class ImageLoader : public BrowserContextKeyedService {
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

  // Checks whether image is a component extension resource. Returns false
  // if a given |resource| does not have a corresponding image in bundled
  // resources. Otherwise fills |resource_id|. This doesn't check if the
  // extension the resource is in is actually a component extension.
  static bool IsComponentExtensionResource(
      const base::FilePath& extension_path,
      const base::FilePath& resource_path,
      int* resource_id);

  // Converts a bitmap image to a PNG representation.
  static scoped_refptr<base::RefCountedMemory> BitmapToMemory(
      const SkBitmap* image);

  explicit ImageLoader(Profile* profile);
  virtual ~ImageLoader();

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
                       const base::Callback<void(const gfx::Image&)>&
                       callback);

  // Load the icon of the given |extension| and return it via |callback| as a
  // gfx::Image. |extension| can be NULL to request the default extension icon.
  // The size in pixels of the returned icon can be chosen with |icon_size| or
  // -1, -1 if no resize is requested. The icon can also be converted to
  // grayscale by setting |grayscale| to true. |match| optionally indicates if
  // it should fallback to smaller or bigger size when choosing the icon source
  // image.
  void LoadExtensionIconAsync(const extensions::Extension* extension,
                              int icon_size,
                              ExtensionIconSet::MatchType match,
                              bool grayscale,
                              const base::Callback<void(const gfx::Image&)>&
                                  callback);

  // Same as LoadExtensionIconAsync() above except the result icon is returned
  // as a PNG image data URL.
  void LoadExtensionIconDataURLAsync(const extensions::Extension* extension,
                                     int icon_size,
                                     ExtensionIconSet::MatchType match,
                                     bool grayscale,
                                     const base::Callback<void(const GURL&)>&
                                         callback);

 private:
  base::WeakPtrFactory<ImageLoader> weak_ptr_factory_;

  Profile* profile_;

  // Task tracker when getting favicon.
  CancelableTaskTracker cancelable_task_tracker_;

  // Cache for the default app icon.
  scoped_ptr<SkBitmap> default_app_icon_;

  // Cache for the default extension icon.
  scoped_ptr<SkBitmap> default_extension_icon_;

  static void LoadImagesOnBlockingPool(
      const std::vector<ImageRepresentation>& info_list,
      const std::vector<SkBitmap>& bitmaps,
      std::vector<LoadResult>* load_result);

  void ReplyBack(
      const std::vector<LoadResult>* load_result,
      const base::Callback<void(const gfx::Image&)>& callback);

  // The sequence for LoadExtensionIconAsync() is the following:
  // 1) It loads the icon image using LoadImageAsync().
  // 2) When it finishes, LoadExtensionIconLoaded() will be called.
  // 3) On success, it will call FinalizeImage(). If it failed, it will call
  // LoadIconFailed(). See below for more on those methods.
  void LoadExtensionIconDone(const extensions::Extension* extension,
                             int icon_size,
                             bool grayscale,
                             const base::Callback<void(const gfx::Image&)>&
                             callback,
                             const gfx::Image& image);

  // Called when the extension doesn't have an icon. We fall back to multiple
  // sources, using the following order:
  //  1) The icons as listed in the extension manifests.
  //  2) If a 16px icon and the extension has a launch URL, see if Chrome has a
  //  corresponding favicon.
  //  3) If still no matches, load the default extension icon.
  void LoadIconFailed(const extensions::Extension* extension,
                      int icon_size,
                      bool grayscale,
                      const base::Callback<void(const gfx::Image&)>& callback);

  // Loads the favicon image for the given |extension|. If the image does not
  // exist, we fall back to the default image.
  void LoadFaviconImage(const extensions::Extension* extension,
                        int icon_size,
                        bool grayscale,
                        const base::Callback<void(const gfx::Image&)>&
                        callback);

  // FaviconService callback. It will call FinalizedImage() on success or try
  // another fallback.
  void OnFaviconDataAvailable(
      const extensions::Extension* extension,
      int icon_size,
      bool grayscale,
      const base::Callback<void(const gfx::Image&)>& callback,
      const chrome::FaviconBitmapResult& bitmap_result);

  // The sequence for LoadDefaultImage() will be the following:
  // 1) LoadDefaultImage() will invoke LoadDefaultImageOnFileThread() on the
  // file thread.
  // 2) LoadDefaultImageOnFileThread() will perform the work, then invoke
  // LoadDefaultImageDone() on the UI thread.
  void LoadDefaultImage(const extensions::Extension* extension,
                        int icon_size,
                        bool grayscale,
                        const base::Callback<void(const gfx::Image&)>&
                            callback);

  // Loads the default image on the file thread.
  void LoadDefaultImageOnFileThread(
      const extensions::Extension* extension,
      int icon_size,
      bool grayscale,
      const base::Callback<void(const gfx::Image&)>& callback);

  // When loading of default image is done, it will call FinalizeImage().
  void LoadDefaultImageDone(const gfx::Image& image,
                            bool grayscale,
                            const base::Callback<void(const gfx::Image&)>&
                                callback);

  // Performs any remaining transformations (like desaturating the |image|),
  // then returns the |image| to the |callback|.
  //
  // The sequence for FinalizeImage() will be the following:
  // 1) FinalizeImage() will invoke FinalizeImageOnFileThread() on the file
  // thread.
  // 2) FinalizeImageOnFileThread() will perform the work, then invoke
  // FinalizeImageDone() on the UI thread.
  void FinalizeImage(const gfx::Image& image,
                     bool grayscale,
                     const base::Callback<void(const gfx::Image&)>& callback);

  // Process the "finalize" operation on the file thread.
  void FinalizeImageOnFileThread(const gfx::Image& image,
                                 bool grayscale,
                                 const base::Callback<void(const gfx::Image&)>&
                                 callback);

  // Called when the "finalize" operation on the file thread is done.
  void FinalizeImageDone(const gfx::Image& image,
                         const base::Callback<void(const gfx::Image&)> &
                         callback);

  // The sequence for LoadExtensionIconDataURLAsync() will be the following:
  // 1) Call LoadExtensionIconAsync() to fetch the icon of the extension.
  // 2) When the icon is loaded, OnIconAvailable() will be called and will
  // invoke ConvertIconToURLOnFileThread() on the file thread.
  // 3) OnIconConvertedToURL() will be called on the UI thread when it's done
  // and will call the callback.
  //
  // LoadExtensionIconDataURLAsync() will use LoadExtensionIconAsync() to get
  // the icon of the extension. The following method will be called when the
  // image has been fetched.
  void OnIconAvailable(const base::Callback<void(const GURL&)>& callback,
                       const gfx::Image& image);

  // ConvertIconToURLOnFileThread() will convert the image to a PNG image data
  // URL on the file thread.
  void ConvertIconToURLOnFileThread(const gfx::Image& image,
                                    const base::Callback<void(const GURL&)>&
                                    callback);

  // This method will call the callback of LoadExtensionIconDataURLAsync() with
  // the result.
  void OnIconConvertedToURL(const GURL& url,
                            const base::Callback<void(const GURL&)>& callback);

  // Returns the bitmap for the default extension icon.
  const SkBitmap* GetDefaultExtensionImage();

  // Returns the bitmap for the default app icon.
  const SkBitmap* GetDefaultAppImage();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_IMAGE_LOADER_H_
