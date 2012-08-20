// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_icon_image.h"

#include <vector>

#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/size.h"

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

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// ExtensionIconImage::Source

class IconImage::Source : public gfx::ImageSkiaSource {
 public:
  explicit Source(IconImage* host);
  virtual ~Source();

  void ResetHost();

 private:
  // gfx::ImageSkiaSource overrides:
  virtual gfx::ImageSkiaRep GetImageForScale(
      ui::ScaleFactor scale_factor) OVERRIDE;

  IconImage* host_;

  DISALLOW_COPY_AND_ASSIGN(Source);
};

IconImage::Source::Source(IconImage* host) : host_(host) {
}

IconImage::Source::~Source() {
}

void IconImage::Source::ResetHost() {
  host_ = NULL;
}

gfx::ImageSkiaRep IconImage::Source::GetImageForScale(
    ui::ScaleFactor scale_factor) {
  if (host_)
    host_->LoadImageForScaleFactor(scale_factor);
  return gfx::ImageSkiaRep();
}

////////////////////////////////////////////////////////////////////////////////
// ExtensionIconImage

IconImage::IconImage(
    const Extension* extension,
    const ExtensionIconSet& icon_set,
    int resource_size_in_dip,
    Observer* observer)
    : extension_(extension),
      icon_set_(icon_set),
      resource_size_in_dip_(resource_size_in_dip),
      desired_size_in_dip_(resource_size_in_dip, resource_size_in_dip),
      observer_(observer),
      source_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)) {
  source_ = new Source(this);
  image_skia_ = gfx::ImageSkia(source_, desired_size_in_dip_);

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::NotificationService::AllSources());
}

IconImage::~IconImage() {
  // |source_| could be NULL if resource does not exist.
  if (source_)
    source_->ResetHost();
}

void IconImage::LoadImageForScaleFactor(ui::ScaleFactor scale_factor) {
  // Do nothing if extension is unloaded.
  if (!extension_)
    return;

  const float scale = ui::GetScaleFactorScale(scale_factor);
  const int resource_size_in_pixel =
      static_cast<int>(resource_size_in_dip_ * scale);

  ExtensionResource resource;
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

  // If there is no resource found, bail out and notify observer of failure.
  if (resource.empty()) {
    if (observer_)
      observer_->OnIconImageLoadFailed(this, scale_factor);
    return;
  }

  int id = tracker_.next_id();
  load_map_[id] = scale_factor;

  std::vector<ImageLoadingTracker::ImageRepresentation> info_list;
  info_list.push_back(ImageLoadingTracker::ImageRepresentation(
      resource,
      ImageLoadingTracker::ImageRepresentation::RESIZE_WHEN_LARGER,
      desired_size_in_dip_.Scale(scale),
      scale_factor));
  tracker_.LoadImages(extension_, info_list, ImageLoadingTracker::DONT_CACHE);
}

void IconImage::OnImageLoaded(const gfx::Image& image,
                              const std::string& extension_id,
                              int index) {
  LoadMap::iterator load_map_it = load_map_.find(index);
  DCHECK(load_map_it != load_map_.end());

  ui::ScaleFactor scale_factor = load_map_it->second;

  load_map_.erase(load_map_it);

  if (image.IsEmpty()) {
    // There waas an error loading the image.
    if (observer_)
      observer_->OnIconImageLoadFailed(this, scale_factor);
    return;
  }

  DCHECK(image.ToImageSkia()->HasRepresentation(scale_factor));
  gfx::ImageSkiaRep rep = image.ToImageSkia()->GetRepresentation(scale_factor);
  DCHECK(!rep.is_null());
  image_skia_.AddRepresentation(rep);

  if (observer_)
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
