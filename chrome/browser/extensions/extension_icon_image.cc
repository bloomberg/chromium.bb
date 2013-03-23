// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_icon_image.h"

#include <vector>

#include "base/bind.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/size.h"
#include "ui/gfx/size_conversions.h"

// The ImageSkia provided by extensions::IconImage contains ImageSkiaReps that
// are computed and updated using the following algorithm (if no default icon
// was supplied, transparent icon is considered the default):
// - |LoadImageForScaleFactors()| searches the extension for an icon of an
//   appropriate size. If the extension doesn't have a icon resource needed for
//   the image representation, the default icon's representation for the
//   requested scale factor is returned by ImageSkiaSource.
// - If the extension has the resource, IconImage tries to load it using
//   ImageLoader.
// - |ImageLoader| is asynchronous.
//  - ImageSkiaSource will initially return transparent image resource of the
//    desired size.
//  - The image will be updated with an appropriate image representation when
//    the |ImageLoader| finishes. The image representation is chosen the same
//    way as in the synchronous case. The observer is notified of the image
//    change, unless the added image representation is transparent (in which
//    case the image had already contained the appropriate image
//    representation).

namespace {

const int kMatchBiggerTreshold = 32;

extensions::ExtensionResource GetExtensionIconResource(
    const extensions::Extension* extension,
    const ExtensionIconSet& icons,
    int size,
    ExtensionIconSet::MatchType match_type) {
  std::string path = icons.Get(size, match_type);
  if (path.empty())
    return extensions::ExtensionResource();

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
    Profile* profile,
    const Extension* extension,
    const ExtensionIconSet& icon_set,
    int resource_size_in_dip,
    const gfx::ImageSkia& default_icon,
    Observer* observer)
    : profile_(profile),
      extension_(extension),
      icon_set_(icon_set),
      resource_size_in_dip_(resource_size_in_dip),
      observer_(observer),
      source_(NULL),
      default_icon_(gfx::ImageSkiaOperations::CreateResizedImage(
          default_icon,
          skia::ImageOperations::RESIZE_BEST,
          gfx::Size(resource_size_in_dip, resource_size_in_dip))),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
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

  extensions::ExtensionResource resource;

  // Find extension resource for non bundled component extensions.
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

  std::vector<ImageLoader::ImageRepresentation> info_list;
  info_list.push_back(ImageLoader::ImageRepresentation(
      resource,
      ImageLoader::ImageRepresentation::ALWAYS_RESIZE,
      gfx::ToFlooredSize(gfx::ScaleSize(
          gfx::Size(resource_size_in_dip_, resource_size_in_dip_), scale)),
      scale_factor));

  extensions::ImageLoader* loader = extensions::ImageLoader::Get(profile_);
  loader->LoadImagesAsync(extension_, info_list,
                          base::Bind(&IconImage::OnImageLoaded,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     scale_factor));

  return gfx::ImageSkiaRep();
}

void IconImage::OnImageLoaded(ui::ScaleFactor scale_factor,
                              const gfx::Image& image_in) {
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
