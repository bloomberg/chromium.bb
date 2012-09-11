// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/image_loading_tracker.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "grit/component_extension_resources_map.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "webkit/glue/image_decoder.h"

using content::BrowserThread;
using extensions::Extension;

namespace {

struct ComponentExtensionResource {
  const char* extension_id;
  const int resource_id;
};

const ComponentExtensionResource kSpecialComponentExtensionResources[] = {
  { extension_misc::kWebStoreAppId, IDR_WEBSTORE_ICON },
  { extension_misc::kChromeAppId, IDR_PRODUCT_LOGO_128 },
};

// Finds special component extension resource id for given extension id.
bool FindSpecialExtensionResourceId(const std::string& extension_id,
                                    int* out_resource_id) {
  for (size_t i = 0; i < arraysize(kSpecialComponentExtensionResources); ++i) {
    if (extension_id == kSpecialComponentExtensionResources[i].extension_id) {
      if (out_resource_id)
        *out_resource_id = kSpecialComponentExtensionResources[i].resource_id;
      return true;
    }
  }

  return false;
}

bool ShouldResizeImageRepresentation(
    ImageLoadingTracker::ImageRepresentation::ResizeCondition resize_method,
    const gfx::Size& decoded_size,
    const gfx::Size& desired_size) {
  switch (resize_method) {
    case ImageLoadingTracker::ImageRepresentation::ALWAYS_RESIZE:
      return decoded_size != desired_size;
    case ImageLoadingTracker::ImageRepresentation::RESIZE_WHEN_LARGER:
      return decoded_size.width() > desired_size.width() ||
             decoded_size.height() > desired_size.height();
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ImageLoadingTracker::Observer

ImageLoadingTracker::Observer::~Observer() {}

////////////////////////////////////////////////////////////////////////////////
// ImageLoadingTracker::ImageRepresentation

ImageLoadingTracker::ImageRepresentation::ImageRepresentation(
    const ExtensionResource& resource,
    ResizeCondition resize_method,
    const gfx::Size& desired_size,
    ui::ScaleFactor scale_factor)
    : resource(resource),
      resize_method(resize_method),
      desired_size(desired_size),
      scale_factor(scale_factor) {
}

ImageLoadingTracker::ImageRepresentation::~ImageRepresentation() {
}

////////////////////////////////////////////////////////////////////////////////
// ImageLoadingTracker::PendingLoadInfo

ImageLoadingTracker::PendingLoadInfo::PendingLoadInfo()
  : extension(NULL),
    cache(CACHE),
    pending_count(0) {
}

ImageLoadingTracker::PendingLoadInfo::~PendingLoadInfo() {}

////////////////////////////////////////////////////////////////////////////////
// ImageLoadingTracker::ImageLoader

// A RefCounted class for loading bitmaps/image reps on the File thread and
// reporting back on the UI thread.
class ImageLoadingTracker::ImageLoader
    : public base::RefCountedThreadSafe<ImageLoader> {
 public:
  explicit ImageLoader(ImageLoadingTracker* tracker)
      : tracker_(tracker) {
    CHECK(BrowserThread::GetCurrentThreadIdentifier(&callback_thread_id_));
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::FILE));
  }

  // Lets this class know that the tracker is no longer interested in the
  // results.
  void StopTracking() {
    tracker_ = NULL;
  }

  // Instructs the loader to load a task on the File thread.
  void LoadImage(const ImageRepresentation& image_info, int id) {
    DCHECK(BrowserThread::CurrentlyOn(callback_thread_id_));
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&ImageLoader::LoadOnFileThread, this, image_info, id));
  }

  void LoadOnFileThread(const ImageRepresentation& image_info, int id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

    // Read the file from disk.
    std::string file_contents;
    FilePath path = image_info.resource.GetFilePath();
    if (path.empty() || !file_util::ReadFileToString(path, &file_contents)) {
      ReportBack(NULL, image_info, gfx::Size(), id);
      return;
    }

    // Decode the bitmap using WebKit's image decoder.
    const unsigned char* data =
        reinterpret_cast<const unsigned char*>(file_contents.data());
    webkit_glue::ImageDecoder decoder;
    scoped_ptr<SkBitmap> decoded(new SkBitmap());
    // Note: This class only decodes bitmaps from extension resources. Chrome
    // doesn't (for security reasons) directly load extension resources provided
    // by the extension author, but instead decodes them in a separate
    // locked-down utility process. Only if the decoding succeeds is the image
    // saved from memory to disk and subsequently used in the Chrome UI.
    // Chrome is therefore decoding bitmaps here that were generated by Chrome.
    *decoded = decoder.Decode(data, file_contents.length());
    if (decoded->empty()) {
      ReportBack(NULL, image_info, gfx::Size(), id);
      return;  // Unable to decode.
    }

    gfx::Size original_size(decoded->width(), decoded->height());
    *decoded = ResizeIfNeeded(*decoded, image_info);

    ReportBack(decoded.release(), image_info, original_size, id);
  }

  // Instructs the loader to load a resource on the File thread.
  void LoadResource(const ImageRepresentation& image_info,
                    int id,
                    int resource_id) {
    DCHECK(BrowserThread::CurrentlyOn(callback_thread_id_));
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&ImageLoader::LoadResourceOnFileThread, this, image_info,
                   id, resource_id));
  }

  void LoadResourceOnFileThread(const ImageRepresentation& image_info,
                                int id,
                                int resource_id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    // TODO(xiyuan): Clean up to use SkBitmap here and in LoadOnFileThread.
    scoped_ptr<SkBitmap> bitmap(new SkBitmap);
    *bitmap = ResourceBundle::GetSharedInstance().GetImageNamed(
        resource_id).AsBitmap();

    *bitmap = ResizeIfNeeded(*bitmap, image_info);
    ReportBack(bitmap.release(), image_info, image_info.desired_size, id);
  }

  void ReportBack(const SkBitmap* bitmap, const ImageRepresentation& image_info,
                  const gfx::Size& original_size, int id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

    BrowserThread::PostTask(
        callback_thread_id_, FROM_HERE,
        base::Bind(&ImageLoader::ReportOnCallingThread, this,
                   bitmap, image_info, original_size, id));
  }

  void ReportOnCallingThread(const SkBitmap* bitmap,
                             const ImageRepresentation& image_info,
                             const gfx::Size& original_size,
                             int id) {
    DCHECK(BrowserThread::CurrentlyOn(callback_thread_id_));

    if (tracker_)
      tracker_->OnBitmapLoaded(bitmap, image_info, original_size, id, true);

    if (bitmap)
      delete bitmap;
  }

 private:
  friend class base::RefCountedThreadSafe<ImageLoader>;
  ~ImageLoader() {}

  SkBitmap ResizeIfNeeded(const SkBitmap& bitmap,
                          const ImageRepresentation& image_info) {
    gfx::Size original_size(bitmap.width(), bitmap.height());
    if (ShouldResizeImageRepresentation(image_info.resize_method,
                                        original_size,
                                        image_info.desired_size)) {
      return skia::ImageOperations::Resize(
          bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
          image_info.desired_size.width(), image_info.desired_size.height());
    }

    return bitmap;
  }

  // The tracker we are loading the bitmap for. If NULL, it means the tracker is
  // no longer interested in the reply.
  ImageLoadingTracker* tracker_;

  // The thread that we need to call back on to report that we are done.
  BrowserThread::ID callback_thread_id_;

  DISALLOW_COPY_AND_ASSIGN(ImageLoader);
};

////////////////////////////////////////////////////////////////////////////////
// ImageLoadingTracker

// static
bool ImageLoadingTracker::IsSpecialBundledExtensionId(
    const std::string& extension_id) {
  int resource_id = -1;
  return FindSpecialExtensionResourceId(extension_id, &resource_id);
}

ImageLoadingTracker::ImageLoadingTracker(Observer* observer)
    : observer_(observer),
      next_id_(0) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::NotificationService::AllSources());
}

ImageLoadingTracker::~ImageLoadingTracker() {
  // The loader is created lazily and is NULL if the tracker is destroyed before
  // any valid image load tasks have been posted.
  if (loader_)
    loader_->StopTracking();
}

void ImageLoadingTracker::LoadImage(const Extension* extension,
                                    const ExtensionResource& resource,
                                    const gfx::Size& max_size,
                                    CacheParam cache) {
  std::vector<ImageRepresentation> info_list;
  info_list.push_back(ImageRepresentation(
      resource,
      ImageRepresentation::RESIZE_WHEN_LARGER,
      max_size,
      ui::SCALE_FACTOR_100P));
  LoadImages(extension, info_list, cache);
}

void ImageLoadingTracker::LoadImages(
    const Extension* extension,
    const std::vector<ImageRepresentation>& info_list,
    CacheParam cache) {
  PendingLoadInfo load_info;
  load_info.extension = extension;
  load_info.cache = cache;
  load_info.extension_id = extension->id();
  load_info.pending_count = info_list.size();
  int id = next_id_++;
  load_map_[id] = load_info;

  for (std::vector<ImageRepresentation>::const_iterator it = info_list.begin();
       it != info_list.end(); ++it) {
    int resource_id = -1;

    // Load resources for special component extensions.
    if (FindSpecialExtensionResourceId(load_info.extension_id, &resource_id)) {
      if (!loader_)
        loader_ = new ImageLoader(this);
      loader_->LoadResource(*it, id, resource_id);
      continue;
    }

    // If we don't have a path we don't need to do any further work, just
    // respond back.
    if (it->resource.relative_path().empty()) {
      OnBitmapLoaded(NULL, *it, it->desired_size, id, false);
      continue;
    }

    DCHECK(extension->path() == it->resource.extension_root());

    // See if the extension has the bitmap already.
    if (extension->HasCachedImage(it->resource, it->desired_size)) {
      SkBitmap bitmap = extension->GetCachedImage(it->resource,
                                                  it->desired_size);
      OnBitmapLoaded(&bitmap, *it, it->desired_size, id, false);
      continue;
    }

    // Instruct the ImageLoader to load this on the File thread. LoadImage and
    // LoadResource do not block.
    if (!loader_)
      loader_ = new ImageLoader(this);

    if (IsComponentExtensionResource(extension, it->resource, resource_id))
      loader_->LoadResource(*it, id, resource_id);
    else
      loader_->LoadImage(*it, id);
  }
}

bool ImageLoadingTracker::IsComponentExtensionResource(
    const Extension* extension,
    const ExtensionResource& resource,
    int& resource_id) const {
  if (extension->location() != Extension::COMPONENT)
    return false;

  FilePath directory_path = extension->path();
  FilePath relative_path = directory_path.BaseName().Append(
      resource.relative_path());

  for (size_t i = 0; i < kComponentExtensionResourcesSize; ++i) {
    FilePath resource_path =
        FilePath().AppendASCII(kComponentExtensionResources[i].name);
    resource_path = resource_path.NormalizePathSeparators();

    if (relative_path == resource_path) {
      resource_id = kComponentExtensionResources[i].value;
      return true;
    }
  }
  return false;
}

void ImageLoadingTracker::OnBitmapLoaded(
    const SkBitmap* bitmap,
    const ImageRepresentation& image_info,
    const gfx::Size& original_size,
    int id,
    bool should_cache) {
  LoadMap::iterator load_map_it = load_map_.find(id);
  DCHECK(load_map_it != load_map_.end());
  PendingLoadInfo* info = &load_map_it->second;

  // Save the pending results.
  DCHECK_GT(info->pending_count, 0u);
  info->pending_count--;
  if (bitmap) {
    info->image_skia.AddRepresentation(gfx::ImageSkiaRep(*bitmap,
                                       image_info.scale_factor));
  }

  // Add to the extension's bitmap cache if requested.
  DCHECK(info->cache != CACHE || info->extension);
  if (should_cache && info->cache == CACHE  && !image_info.resource.empty() &&
      !info->extension->HasCachedImage(image_info.resource, original_size)) {
    info->extension->SetCachedImage(image_info.resource,
                                    bitmap ? *bitmap : SkBitmap(),
                                    original_size);
  }

  // If all pending bitmaps are done then report back.
  if (info->pending_count == 0) {
    gfx::Image image;
    std::string extension_id = info->extension_id;

    if (!info->image_skia.isNull()) {
      info->image_skia.MakeThreadSafe();
      image = gfx::Image(info->image_skia);
    }

    load_map_.erase(load_map_it);

    // ImageLoadingTracker might be deleted after the callback so don't do
    // anything after this statement.
    observer_->OnImageLoaded(image, extension_id, id);
  }
}

void ImageLoadingTracker::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_EXTENSION_UNLOADED);

  const Extension* extension =
      content::Details<extensions::UnloadedExtensionInfo>(details)->extension;

  // Remove reference to this extension from all pending load entries. This
  // ensures we don't attempt to cache the bitmap when the load completes.
  for (LoadMap::iterator i = load_map_.begin(); i != load_map_.end(); ++i) {
    PendingLoadInfo* info = &i->second;
    if (info->extension == extension) {
      info->extension = NULL;
      info->cache = DONT_CACHE;
    }
  }
}
