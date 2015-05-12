// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/launcher_search/extension_badged_icon_image.h"

#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/launcher_search_provider/error_reporter.h"
#include "extensions/common/constants.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

const int kTruncatedIconUrlMaxSize = 100;
const char kWarningMessagePrefix[] =
    "[chrome.launcherSearchProvider.setSearchResults]";

class BadgedIconSource : public gfx::CanvasImageSource {
 public:
  BadgedIconSource(const gfx::ImageSkia& custom_icon,
                   const gfx::ImageSkia& extension_icon,
                   const gfx::Size& icon_size)
      : CanvasImageSource(icon_size, false), custom_icon_(custom_icon) {
    // Badged icon size is 2/3 of custom icon.
    int badge_dimension = size().width() * 2 / 3;
    gfx::Size badge_size = gfx::Size(badge_dimension, badge_dimension);
    resized_extension_icon_ = gfx::ImageSkiaOperations::CreateResizedImage(
        extension_icon, skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
        badge_size);
  }

  void Draw(gfx::Canvas* canvas) override {
    canvas->DrawImageInt(custom_icon_, 0, 0);
    canvas->DrawImageInt(
        resized_extension_icon_,
        size().width() - resized_extension_icon_.size().width(),
        size().height() - resized_extension_icon_.size().height());
  }

 private:
  gfx::ImageSkia custom_icon_;
  gfx::ImageSkia resized_extension_icon_;

  DISALLOW_COPY_AND_ASSIGN(BadgedIconSource);
};

}  // namespace

namespace app_list {

ExtensionBadgedIconImage::ExtensionBadgedIconImage(
    const GURL& icon_url,
    Profile* profile,
    const extensions::Extension* extension,
    const int icon_dimension,
    scoped_ptr<chromeos::launcher_search_provider::ErrorReporter>
        error_reporter)
    : profile_(profile),
      extension_(extension),
      icon_url_(icon_url),
      icon_size_(icon_dimension, icon_dimension),
      error_reporter_(error_reporter.Pass()) {
}

ExtensionBadgedIconImage::~ExtensionBadgedIconImage() {
}

void ExtensionBadgedIconImage::LoadResources() {
  // Loads extension icon image.
  extension_icon_image_ = LoadExtensionIcon();
  Update();

  // If valid icon_url is provided as chrome-extension scheme with the host of
  // |extension|, load custom icon.
  if (icon_url_.is_empty()) {
    return;
  }

  if (!icon_url_.is_valid() ||
      !icon_url_.SchemeIs(extensions::kExtensionScheme) ||
      icon_url_.host() != extension_->id()) {
    std::vector<std::string> params;
    params.push_back(kWarningMessagePrefix);
    params.push_back(GetTruncatedIconUrl(kTruncatedIconUrlMaxSize));
    params.push_back(extensions::kExtensionScheme);
    params.push_back(extension_->id());
    error_reporter_->Warn(ReplaceStringPlaceholders(
        "$1 Invalid icon URL: $2. Must have a valid URL within $3://$4.",
        params, nullptr));
    return;
  }

  // Update() is called when custom icon is loaded.
  LoadIconResourceFromExtension();
}

void ExtensionBadgedIconImage::AddObserver(Observer* observer) {
  observers_.insert(observer);
}

void ExtensionBadgedIconImage::RemoveObserver(Observer* observer) {
  observers_.erase(observer);
}

const gfx::ImageSkia& ExtensionBadgedIconImage::GetIconImage() const {
  return badged_icon_image_;
}

void ExtensionBadgedIconImage::OnExtensionIconChanged(
    const gfx::ImageSkia& image) {
  extension_icon_image_ = image;
  Update();
}

void ExtensionBadgedIconImage::OnCustomIconLoaded(const gfx::ImageSkia& image) {
  if (image.isNull()) {
    std::vector<std::string> params;
    params.push_back(kWarningMessagePrefix);
    params.push_back(GetTruncatedIconUrl(kTruncatedIconUrlMaxSize));
    error_reporter_->Warn(ReplaceStringPlaceholders(
        "$1 Failed to load icon URL: $2", params, nullptr));
    return;
  }

  custom_icon_image_ = image;
  Update();
}

void ExtensionBadgedIconImage::Update() {
  // If extension_icon_image is not available, return immediately.
  if (extension_icon_image_.isNull())
    return;

  // When custom icon image is not available, simply use extension icon image
  // without badge.
  if (custom_icon_image_.isNull()) {
    SetIconImage(extension_icon_image_);
    return;
  }

  // Create badged icon image.
  gfx::ImageSkia badged_icon_image(
      new BadgedIconSource(custom_icon_image_, extension_icon_image_,
                           icon_size_),
      icon_size_);
  SetIconImage(badged_icon_image);
}

void ExtensionBadgedIconImage::SetIconImage(const gfx::ImageSkia& icon_image) {
  badged_icon_image_ = icon_image;

  for (auto* observer : observers_) {
    observer->OnIconImageChanged(this);
  }
}

std::string ExtensionBadgedIconImage::GetTruncatedIconUrl(
    const uint32 max_size) {
  CHECK(max_size > 3);

  if (icon_url_.spec().size() <= max_size)
    return icon_url_.spec();

  std::string truncated_url;
  base::TruncateUTF8ToByteSize(icon_url_.spec(), max_size - 3, &truncated_url);
  truncated_url.append("...");
  return truncated_url;
}

}  // namespace app_list
