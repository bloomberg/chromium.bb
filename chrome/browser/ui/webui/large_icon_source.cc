// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/large_icon_source.h"

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/common/favicon/large_icon_url_parser.h"
#include "chrome/common/url_constants.h"
#include "components/favicon/core/fallback_icon_service.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "net/url_request/url_request.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"

namespace {

const int kDefaultLargeIconSize = 96;
const int kMaxLargeIconSize = 192;  // Arbitrary bound to safeguard endpoint.

const double kMaxBackgroundLuminance = 0.67;
const SkColor kDarkGray = SkColorSetRGB(0x78, 0x78, 0x78);
const SkColor kTextColor = SK_ColorWHITE;
const SkColor kDefaultBackgroundColor = kDarkGray;

}  // namespace

LargeIconSource::IconRequest::IconRequest() : size(kDefaultLargeIconSize) {
}

LargeIconSource::IconRequest::IconRequest(
    const content::URLDataSource::GotDataCallback& callback_in,
    const GURL& url_in,
    int size_in)
    : callback(callback_in),
      url(url_in),
      size(size_in) {
}

LargeIconSource::IconRequest::~IconRequest() {
}

LargeIconSource::LargeIconSource(
    favicon::FaviconService* favicon_service,
    favicon::FallbackIconService* fallback_icon_service)
    : favicon_service_(favicon_service),
      fallback_icon_service_(fallback_icon_service) {
  large_icon_types_.push_back(favicon_base::IconType::FAVICON);
  large_icon_types_.push_back(favicon_base::IconType::TOUCH_ICON);
  large_icon_types_.push_back(favicon_base::IconType::TOUCH_PRECOMPOSED_ICON);
}

LargeIconSource::~LargeIconSource() {
}

std::string LargeIconSource::GetSource() const {
  return chrome::kChromeUILargeIconHost;
}

void LargeIconSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  if (!favicon_service_) {
    SendNotFoundResponse(callback);
    return;
  }

  LargeIconUrlParser parser;
  bool success = parser.Parse(path);
  if (!success ||
      parser.size_in_pixels() <= 0 ||
      parser.size_in_pixels() > kMaxLargeIconSize) {
    SendNotFoundResponse(callback);
    return;
  }

  GURL url(parser.url_string());
  if (!url.is_valid()) {
    SendNotFoundResponse(callback);
    return;
  }

  favicon_service_->GetLargestRawFaviconForPageURL(
      url,
      large_icon_types_,
      parser.size_in_pixels(),
      base::Bind(
          &LargeIconSource::OnIconDataAvailable,
          base::Unretained(this),
          IconRequest(callback, url, parser.size_in_pixels())),
      &cancelable_task_tracker_);
}

std::string LargeIconSource::GetMimeType(const std::string&) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

bool LargeIconSource::ShouldReplaceExistingSource() const {
  // Leave the existing DataSource in place, otherwise we'll drop any pending
  // requests on the floor.
  return false;
}

bool LargeIconSource::ShouldServiceRequest(
    const net::URLRequest* request) const {
  if (request->url().SchemeIs(chrome::kChromeSearchScheme))
    return InstantIOContext::ShouldServiceRequest(request);
  return URLDataSource::ShouldServiceRequest(request);
}

void LargeIconSource::OnIconDataAvailable(
    const IconRequest& request,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  if (!bitmap_result.is_valid()) {
    SendDefaultFallbackIcon(request);
    return;
  }

  // If we found a bitmap, but it's smaller than the requested size, we
  // generate a fallback using the dominant color from the too-small bitmap.
  // We adjust the luminance of the background so we can put light text over it.
  if (bitmap_result.pixel_size.width() < request.size ||
      bitmap_result.pixel_size.height() < request.size) {
    SkColor background =
        color_utils::CalculateKMeanColorOfPNG(bitmap_result.bitmap_data);
    color_utils::HSL background_hsl;
    color_utils::SkColorToHSL(background, &background_hsl);
    background_hsl.l = std::min(background_hsl.l, kMaxBackgroundLuminance);
    background = color_utils::HSLToSkColor(background_hsl, SK_AlphaOPAQUE);

    // Now we can construct the fallback icon.
    SendFallbackIcon(request, kTextColor, background);
    return;
  }

  request.callback.Run(bitmap_result.bitmap_data.get());
}

void LargeIconSource::SendDefaultFallbackIcon(const IconRequest& request) {
  SendFallbackIcon(request, kTextColor, kDefaultBackgroundColor);
}

void LargeIconSource::SendFallbackIcon(const IconRequest& request,
                                       SkColor text_color,
                                       SkColor background_color) {
  if (!fallback_icon_service_) {
    SendNotFoundResponse(request.callback);
    return;
  }
  favicon_base::FallbackIconStyle style;
  style.background_color = background_color;
  style.text_color = text_color;
  style.font_size_ratio = 0.44;
  style.roundness = 0;  // Square. Round corners can be applied by JavaScript.
  std::vector<unsigned char> bitmap_data =
      fallback_icon_service_->RenderFallbackIconBitmap(
          request.url, request.size, style);
  request.callback.Run(base::RefCountedBytes::TakeVector(&bitmap_data));
}

void LargeIconSource::SendNotFoundResponse(
    const content::URLDataSource::GotDataCallback& callback) {
  callback.Run(nullptr);
}
