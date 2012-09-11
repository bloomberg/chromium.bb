// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_icon_image.h"

#include <vector>

#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/size.h"

// The ImageSkia provided by extensions::IconImage contains ImageSkiaReps that
// are computed and updated using the following algorithm (if no default icon
// was supplied, transparent icon is considered the default):
// - |LoadImageForScaleFactors()| searches the extension for an icon of an
//   appropriate size. If the extension doesn't have a icon resource needed for
//   the image representation, the default icon's representation for the
//   requested scale factor is returned by ImageSkiaSource.
// - If the extension has the resource, IconImage tries to load it using
//   ImageLoadingTracker.
// - |ImageLoadingTracker| may return both synchronously and asynchronously.
// 1. |ImageLoadingTracker| is synchronous.
//  - If image representation resource is successfully loaded, the
//    representation returned by ImageSkiaSource is created from the loaded
//    bitmap.
//  - If resource loading fails, ImageSkiaSource returns default icon's
//    representation.
// 2. |ImageLoadingTracker| is asynchronous.
//  - ImageSkiaSource will initially return transparent image resource of the
//    desired size.
//  - The image will be updated with an appropriate image representation when
//    the |ImageLoadingTracker| finishes. The image representation is chosen
//    the same way as in the synchronous case. The observer is notified of the
//    image change, unless the added image representation is transparent (in
//    which case the image had already contained the appropriate image
//    representation).

namespace {

const int kMatchBiggerTreshold = 32;

ExtensionResource GetExtensionIconResource(
    const extensions::Extension* extension,
    const ExtensionIconSet& icons,
    int size,
    ExtensionIconSet::MatchType match_type) {
  std::string path = icons.Get(size, match_type);
  if (path.empty())
    return ExtensionResource();

  return extension->GetResource(path);
}

class BlankImageSource : public gfx::CanvasImageSource {
 public:
  explicit BlankImageSource(const gfx::Size& size_in_dip)
      : CanvasImageSource(size_in_dip, /*is_opaque =*/ false) {
  }
  virtual ~BlankImageSource() {}

 private:
  // gfx::CanvasImageSource overrides:
  virtual void Draw(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawColor(SkColorSetARGB(0, 0, 0, 0));
  }

  DISALLOW_COPY_AND_ASSIGN(BlankImageSource);
};

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// IconImage::Source

class IconImage::Source : public gfx::ImageSkiaSource {
 public:
  Source(IconImage* host, const gfx::Size& size_in_dip);
  virtual ~Source();

  void ResetHost();

 private:
  // gfx::ImageSkiaSource overrides:
  virtual gfx::ImageSkiaRep GetImageForScale(
      ui::ScaleFactor scale_factor) OVERRIDE;

  // Used to load images, possibly asynchronously. NULLed out when the IconImage
  // is destroyed.
  IconImage* host_;

  // Image whose representations will be used until |host_| loads the real
  // representations for the image.
  gfx::ImageSkia blank_image_;

  DISALLOW_COPY_AND_ASSIGN(Source);
};

IconImage::Source::Source(IconImage* host, const gfx::Size& size_in_dip)
    : host_(host),
      blank_image_(new BlankImageSource(size_in_dip), size_in_dip) {
}

IconImage::Source::~Source() {
}

void IconImage::Source::ResetHost() {
  host_ = NULL;
}

gfx::ImageSkiaRep IconImage::Source::GetImageForScale(
    ui::ScaleFactor scale_factor) {
  gfx::ImageSkiaRep representation;
  if (host_)
    representation = host_->LoadImageForScaleFactor(scale_factor);

  if (!representation.is_null())
    return representation;

  return blank_image_.GetRepresentation(scale_factor);
}

////////////////////////////////////////////////////////////////////////////////
// IconImage

IconImage::IconImage(
    const Extension* extension,
    const ExtensionIconSet& icon_set,
    int resource_size_in_dip,
    const gfx::ImageSkia& default_icon,
    Observer* observer)
    : extension_(extension),
      icon_set_(icon_set),
      resource_size_in_dip_(resource_size_in_dip),
      observer_(observer),
      source_(NULL),
      default_icon_(default_icon),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)) {
  gfx::Size resource_size(resource_size_in_dip, resource_size_in_dip);
  source_ = new Source(this, resource_size);
  image_skia_ = gfx::ImageSkia(source_, resource_size);

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::NotificationService::AllSources());
}

IconImage::~IconImage() {
  source_->ResetHost();
}

gfx::ImageSkiaRep IconImage::LoadImageForScaleFactor(
    ui::ScaleFactor scale_factor) {
  // Do nothing if extension is unloaded.
  if (!extension_)
    return gfx::ImageSkiaRep();

  const float scale = ui::GetScaleFactorScale(scale_factor);
  const int resource_size_in_pixel =
      static_cast<int>(resource_size_in_dip_ * scale);

  ExtensionResource resource;

  // Find extension resource for non bundled component extensions.
  if (!ImageLoadingTracker::IsSpecialBundledExtensionId(extension_->id())) {
    // We try loading bigger image only if resource size is >= 32.
    if (resource_size_in_pixel >= kMatchBiggerTreshold) {
      resource = GetExtensionIconResource(extension_, icon_set_,
          resource_size_in_pixel, ExtensionIconSet::MATCH_BIGGER);
    }

    // If resource is not found by now, try matching smaller one.
    if (resource.empty()) {
      resource = GetExtensionIconResource(extension_, icon_set_,
          resource_size_in_pixel, ExtensionIconSet::MATCH_SMALLER);
    }

    // If there is no resource found, return default icon.
    if (resource.empty())
      return default_icon_.GetRepresentation(scale_factor);
  }

  int id = tracker_.next_id();
  load_map_[id].scale_factor = scale_factor;
  load_map_[id].is_async = false;

  std::vector<ImageLoadingTracker::ImageRepresentation> info_list;
  info_list.push_back(ImageLoadingTracker::ImageRepresentation(
      resource,
      ImageLoadingTracker::ImageRepresentation::RESIZE_WHEN_LARGER,
      gfx::Size(resource_size_in_dip_, resource_size_in_dip_).Scale(scale),
      scale_factor));
  tracker_.LoadImages(extension_, info_list, ImageLoadingTracker::DONT_CACHE);

  // If we have not received |OnImageLoaded|, image load request is
  // asynchronous.
  if (load_map_.find(id) != load_map_.end())
    load_map_[id].is_async = true;

  // If LoadImages returned synchronously and the requested image rep is cached
  // in the extension, return the cached image rep.
  if (image_skia_.HasRepresentation(scale_factor))
    return image_skia_.GetRepresentation(scale_factor);

  return gfx::ImageSkiaRep();
}

void IconImage::OnImageLoaded(const gfx::Image& image_in,
                              const std::string& extension_id,
                              int index) {
  LoadMap::iterator load_map_it = load_map_.find(index);
  DCHECK(load_map_it != load_map_.end());

  ui::ScaleFactor scale_factor = load_map_it->second.scale_factor;
  bool is_async = load_map_it->second.is_async;

  load_map_.erase(load_map_it);

  const gfx::ImageSkia* image =
      image_in.IsEmpty() ? &default_icon_ : image_in.ToImageSkia();

  // Maybe default icon was not set.
  if (image->isNull())
    return;

  gfx::ImageSkiaRep rep = image->GetRepresentation(scale_factor);
  DCHECK(!rep.is_null());
  DCHECK_EQ(scale_factor, rep.scale_factor());

  // Remove old representation if there is one.
  image_skia_.RemoveRepresentation(rep.scale_factor());
  image_skia_.AddRepresentation(rep);

  // If |tracker_| called us synchronously the image did not really change from
  // the observer's perspective, since the initial image representation is
  // returned synchronously.
  if (is_async && observer_)
    observer_->OnExtensionIconImageChanged(this);
}

void IconImage::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_EXTENSION_UNLOADED);

  const Extension* extension =
      content::Details<extensions::UnloadedExtensionInfo>(details)->extension;

  if (extension_ == extension)
    extension_ = NULL;
}

}  // namespace extensions
