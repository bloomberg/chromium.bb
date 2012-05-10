// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/image_loading_tracker.h"

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
#include "ui/gfx/image/image.h"
#include "webkit/glue/image_decoder.h"

using content::BrowserThread;

////////////////////////////////////////////////////////////////////////////////
// ImageLoadingTracker::Observer

ImageLoadingTracker::Observer::~Observer() {}

////////////////////////////////////////////////////////////////////////////////
// ImageLoadingTracker::ImageInfo

ImageLoadingTracker::ImageInfo::ImageInfo(
    const ExtensionResource& resource, gfx::Size max_size)
    : resource(resource), max_size(max_size) {
}

ImageLoadingTracker::ImageInfo::~ImageInfo() {
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

// A RefCounted class for loading images on the File thread and reporting back
// on the UI thread.
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
  void LoadImage(const ExtensionResource& resource,
                 const gfx::Size& max_size,
                 int id) {
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::FILE));
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&ImageLoader::LoadOnFileThread, this, resource,
                   max_size, id));
  }

  void LoadOnFileThread(const ExtensionResource& resource,
                        const gfx::Size& max_size,
                        int id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

    // Read the file from disk.
    std::string file_contents;
    FilePath path = resource.GetFilePath();
    if (path.empty() || !file_util::ReadFileToString(path, &file_contents)) {
      ReportBack(NULL, resource, gfx::Size(), id);
      return;
    }

    // Decode the image using WebKit's image decoder.
    const unsigned char* data =
        reinterpret_cast<const unsigned char*>(file_contents.data());
    webkit_glue::ImageDecoder decoder;
    scoped_ptr<SkBitmap> decoded(new SkBitmap());
    *decoded = decoder.Decode(data, file_contents.length());
    if (decoded->empty()) {
      ReportBack(NULL, resource, gfx::Size(), id);
      return;  // Unable to decode.
    }

    gfx::Size original_size(decoded->width(), decoded->height());

    if (decoded->width() > max_size.width() ||
        decoded->height() > max_size.height()) {
      // The bitmap is too big, re-sample.
      *decoded = skia::ImageOperations::Resize(
          *decoded, skia::ImageOperations::RESIZE_LANCZOS3,
          max_size.width(), max_size.height());
    }

    ReportBack(decoded.release(), resource, original_size, id);
  }

  // Instructs the loader to load a resource on the File thread.
  void LoadResource(const ExtensionResource& resource,
                    const gfx::Size& max_size,
                    int id,
                    int resource_id) {
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::FILE));
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&ImageLoader::LoadResourceOnFileThread, this, resource,
                   max_size, id, resource_id));
  }

  void LoadResourceOnFileThread(const ExtensionResource& resource,
                                const gfx::Size& max_size,
                                int id,
                                int resource_id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    SkBitmap* image = ExtensionIconSource::LoadImageByResourceId(
        resource_id);
    ReportBack(image, resource, max_size, id);
  }

  void ReportBack(SkBitmap* image, const ExtensionResource& resource,
                  const gfx::Size& original_size, int id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

    BrowserThread::PostTask(
        callback_thread_id_, FROM_HERE,
        base::Bind(&ImageLoader::ReportOnUIThread, this,
                   image, resource, original_size, id));
  }

  void ReportOnUIThread(SkBitmap* image, const ExtensionResource& resource,
                        const gfx::Size& original_size, int id) {
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::FILE));

    if (tracker_)
      tracker_->OnImageLoaded(image, resource, original_size, id, true);

    delete image;
  }

 private:
  friend class base::RefCountedThreadSafe<ImageLoader>;
  ~ImageLoader() {}

  // The tracker we are loading the image for. If NULL, it means the tracker is
  // no longer interested in the reply.
  ImageLoadingTracker* tracker_;

  // The thread that we need to call back on to report that we are done.
  BrowserThread::ID callback_thread_id_;

  DISALLOW_COPY_AND_ASSIGN(ImageLoader);
};

////////////////////////////////////////////////////////////////////////////////
// ImageLoadingTracker

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
  std::vector<ImageInfo> info_list;
  info_list.push_back(ImageInfo(resource, max_size));
  LoadImages(extension, info_list, cache);
}

void ImageLoadingTracker::LoadImages(const Extension* extension,
                                     const std::vector<ImageInfo>& info_list,
                                     CacheParam cache) {
  PendingLoadInfo load_info;
  load_info.extension = extension;
  load_info.cache = cache;
  load_info.extension_id = extension->id();
  load_info.pending_count = info_list.size();
  int id = next_id_++;
  load_map_[id] = load_info;

  for (std::vector<ImageInfo>::const_iterator it = info_list.begin();
       it != info_list.end(); ++it) {
    // Load resources for WebStore component extension.
    if (load_info.extension_id == extension_misc::kWebStoreAppId) {
      if (!loader_)
        loader_ = new ImageLoader(this);
      loader_->LoadResource(it->resource, it->max_size, id, IDR_WEBSTORE_ICON);
      continue;
    }

    // If we don't have a path we don't need to do any further work, just
    // respond back.
    if (it->resource.relative_path().empty()) {
      OnImageLoaded(NULL, it->resource, it->max_size, id, false);
      continue;
    }

    DCHECK(extension->path() == it->resource.extension_root());

    // See if the extension has the image already.
    if (extension->HasCachedImage(it->resource, it->max_size)) {
      SkBitmap image = extension->GetCachedImage(it->resource, it->max_size);
      OnImageLoaded(&image, it->resource, it->max_size, id, false);
      continue;
    }

    // Instruct the ImageLoader to load this on the File thread. LoadImage and
    // LoadResource do not block.
    if (!loader_)
      loader_ = new ImageLoader(this);

    int resource_id;
    if (IsComponentExtensionResource(extension, it->resource, resource_id))
      loader_->LoadResource(it->resource, it->max_size, id, resource_id);
    else
      loader_->LoadImage(it->resource, it->max_size, id);
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

void ImageLoadingTracker::OnImageLoaded(
    SkBitmap* image,
    const ExtensionResource& resource,
    const gfx::Size& original_size,
    int id,
    bool should_cache) {
  LoadMap::iterator load_map_it = load_map_.find(id);
  DCHECK(load_map_it != load_map_.end());
  PendingLoadInfo* info = &load_map_it->second;

  // Save the pending results.
  DCHECK(info->pending_count > 0);
  info->pending_count--;
  if (image)
    info->bitmaps.push_back(*image);

  // Add to the extension's image cache if requested.
  DCHECK(info->cache != CACHE || info->extension);
  if (should_cache && info->cache == CACHE  &&
      !info->extension->HasCachedImage(resource, original_size)) {
    info->extension->SetCachedImage(resource, image ? *image : SkBitmap(),
                                    original_size);
  }

  // If all pending images are done then report back.
  if (info->pending_count == 0) {
    gfx::Image image;
    std::string extension_id = info->extension_id;

    if (info->bitmaps.size() > 0) {
      std::vector<const SkBitmap*> bitmaps;
      for (std::vector<SkBitmap>::const_iterator it = info->bitmaps.begin();
           it != info->bitmaps.end(); ++it) {
        // gfx::Image takes ownership of this bitmap.
        bitmaps.push_back(new SkBitmap(*it));
      }
      image = gfx::Image(bitmaps);
    }

    load_map_.erase(load_map_it);

    // ImageLoadingTracker might be deleted after the callback so don't
    // anything after this statement.
    observer_->OnImageLoaded(image, extension_id, id);
  }
}

void ImageLoadingTracker::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_EXTENSION_UNLOADED);

  const Extension* extension =
      content::Details<UnloadedExtensionInfo>(details)->extension;

  // Remove reference to this extension from all pending load entries. This
  // ensures we don't attempt to cache the image when the load completes.
  for (LoadMap::iterator i = load_map_.begin(); i != load_map_.end(); ++i) {
    PendingLoadInfo* info = &i->second;
    if (info->extension == extension) {
      info->extension = NULL;
      info->cache = DONT_CACHE;
    }
  }
}
