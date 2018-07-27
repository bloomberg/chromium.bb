// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/ntp_icon_source.h"

#include <stddef.h>
#include <algorithm>
#include <cmath>

#include "base/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/platform_locale_settings.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/top_sites.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_service.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/escape.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace {

// Delimiter in the url that looks for the size specification.
const char kSizeParameter[] = "size/";

// Size of the fallback icon (letter + circle), in dp.
const int kFallbackIconSizeDip = 48;

// URL to the server favicon service. "alt=404" means the service will return a
// 404 if an icon can't be found.
const char kServerFaviconURL[] =
    "https://s2.googleusercontent.com/s2/favicons?domain_url=%s&alt=404&sz=32";

// Used to parse the specification from the path.
struct ParsedNtpIconPath {
  // The URL for which the icon is being requested.
  GURL url;

  // The size of the requested icon in dip.
  int size_in_dip = 0;

  // The device scale factor of the requested icon.
  float device_scale_factor = 1.0;
};

// Returns true if |search| is a substring of |path| which starts at
// |start_index|.
bool HasSubstringAt(const std::string& path,
                    size_t start_index,
                    const std::string& search) {
  return path.compare(start_index, search.length(), search) == 0;
}

// Parses the path after chrome-search://ntpicon/. Example path is
// "size/24@2x/https://cnn.com".
const ParsedNtpIconPath ParseNtpIconPath(const std::string& path) {
  ParsedNtpIconPath parsed;
  parsed.url = GURL();
  parsed.size_in_dip = gfx::kFaviconSize;
  parsed.device_scale_factor = 1.0f;

  if (path.empty())
    return parsed;

  // Size specification has to be present.
  size_t parsed_index = 0;
  if (!HasSubstringAt(path, parsed_index, kSizeParameter))
    return parsed;

  parsed_index += strlen(kSizeParameter);
  size_t slash = path.find("/", parsed_index);
  if (slash == std::string::npos)
    return parsed;

  // Parse the size spec (e.g. "24@2x")
  size_t scale_delimiter = path.find("@", parsed_index);
  std::string size_str =
      path.substr(parsed_index, scale_delimiter - parsed_index);
  std::string scale_str =
      path.substr(scale_delimiter + 1, slash - scale_delimiter - 1);

  if (!base::StringToInt(size_str, &parsed.size_in_dip))
    return parsed;

  if (!scale_str.empty())
    webui::ParseScaleFactor(scale_str, &parsed.device_scale_factor);

  parsed_index = slash + 1;

  parsed.url = GURL(path.substr(parsed_index));
  return parsed;
}

// Draws a circle of a given |size| in the |canvas| and fills it with
// |background_color|.
void DrawCircleInCanvas(gfx::Canvas* canvas,
                        int size,
                        SkColor background_color) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(background_color);
  int corner_radius = static_cast<int>(size * 0.5 + 0.5);
  canvas->DrawRoundRect(gfx::Rect(0, 0, size, size), corner_radius, flags);
}

// Will paint the appropriate letter in the center of specified |canvas| of
// given |size|.
void DrawFallbackIconLetter(const GURL& icon_url,
                            int size,
                            gfx::Canvas* canvas) {
  // Get the appropriate letter to draw, then eventually draw it.
  base::string16 icon_text = favicon::GetFallbackIconText(icon_url);
  if (icon_text.empty())
    return;

  const double kDefaultFontSizeRatio = 0.44;
  int font_size = static_cast<int>(size * kDefaultFontSizeRatio);
  if (font_size <= 0)
    return;

  // TODO(crbug.com/853780): Adjust the text color according to the background
  // color.
  canvas->DrawStringRectWithFlags(
      icon_text,
      gfx::FontList({l10n_util::GetStringUTF8(IDS_SANS_SERIF_FONT_FAMILY)},
                    gfx::Font::NORMAL, font_size, gfx::Font::Weight::NORMAL),
      SK_ColorWHITE, gfx::Rect(0, 0, size, size),
      gfx::Canvas::TEXT_ALIGN_CENTER);
}

// Will draw |bitmap| in the center of the |canvas| of a given |size|.
// |bitmap| keeps its size.
void DrawFavicon(const SkBitmap& bitmap, gfx::Canvas* canvas, int size) {
  int x_origin = (size - bitmap.width()) / 2;
  int y_origin = (size - bitmap.height()) / 2;
  canvas->DrawImageInt(gfx::ImageSkia(gfx::ImageSkiaRep(bitmap, /*scale=*/1.0)),
                       x_origin, y_origin);
}

// Returns a color that based on the hash of |icon_url|'s origin.
SkColor GetBackgroundColorForUrl(const GURL& icon_url) {
  if (!icon_url.is_valid())
    return SK_ColorGRAY;

  unsigned char hash[20];
  const std::string origin = icon_url.GetOrigin().spec();
  base::SHA1HashBytes(reinterpret_cast<const unsigned char*>(origin.c_str()),
                      origin.size(), hash);
  return SkColorSetRGB(hash[0], hash[1], hash[2]);
}

// For the given |icon_url|, will render a fallback icon with an appropriate
// letter in a circle.
std::vector<unsigned char> RenderIconBitmap(const GURL& icon_url,
                                            const SkBitmap& favicon,
                                            int size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size, false);
  cc::SkiaPaintCanvas paint_canvas(bitmap);
  gfx::Canvas canvas(&paint_canvas, 1.f);
  canvas.DrawColor(SK_ColorTRANSPARENT, SkBlendMode::kSrc);
  if (!favicon.empty()) {
    constexpr SkColor kFaviconBackground = SkColorSetRGB(0xF1, 0xF3, 0xF4);
    DrawCircleInCanvas(&canvas, size, /*background_color=*/kFaviconBackground);
    DrawFavicon(favicon, &canvas, size);
  } else {
    SkColor background_color = GetBackgroundColorForUrl(icon_url);
    // If luminance is too high, the white text will become unreadable. Invert
    // the background color to achieve better constrast. The constant comes
    // W3C Accessibility standards.
    if (color_utils::GetRelativeLuminance(background_color) > 0.179f)
      background_color = color_utils::InvertColor(background_color);

    DrawCircleInCanvas(&canvas, size, background_color);
    DrawFallbackIconLetter(icon_url, size, &canvas);
  }
  std::vector<unsigned char> bitmap_data;
  bool result = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &bitmap_data);
  DCHECK(result);
  return bitmap_data;
}

}  // namespace

struct NtpIconSource::NtpIconRequest {
  NtpIconRequest(const content::URLDataSource::GotDataCallback& cb,
                 const GURL& path,
                 int icon_size_in_pixels,
                 float scale)
      : callback(cb),
        path(path),
        icon_size_in_pixels(icon_size_in_pixels),
        device_scale_factor(scale) {}
  NtpIconRequest(const NtpIconRequest& other) = default;
  ~NtpIconRequest() {}

  content::URLDataSource::GotDataCallback callback;
  GURL path;
  int icon_size_in_pixels;
  float device_scale_factor;
};

NtpIconSource::NtpIconSource(Profile* profile)
    : profile_(profile),
      image_fetcher_(std::make_unique<image_fetcher::ImageFetcherImpl>(
          std::make_unique<suggestions::ImageDecoderImpl>(),
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetURLLoaderFactoryForBrowserProcess())),
      weak_ptr_factory_(this) {
  image_fetcher_->SetDataUseServiceName(
      data_use_measurement::DataUseUserData::NTP_TILES);
}

NtpIconSource::~NtpIconSource() = default;

std::string NtpIconSource::GetSource() const {
  return chrome::kChromeUINewTabIconHost;
}

void NtpIconSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);

  const ParsedNtpIconPath parsed = ParseNtpIconPath(path);

  if (parsed.url.is_valid()) {
    int icon_size_in_pixels =
        std::ceil(parsed.size_in_dip * parsed.device_scale_factor);
    NtpIconRequest request(callback, parsed.url, icon_size_in_pixels,
                           parsed.device_scale_factor);

    // Check if the requested URL is part of the prepopulated pages (currently,
    // only the Web Store).
    scoped_refptr<history::TopSites> top_sites =
        TopSitesFactory::GetForProfile(profile_);
    if (top_sites) {
      for (const auto& prepopulated_page : top_sites->GetPrepopulatedPages()) {
        if (parsed.url == prepopulated_page.most_visited.url) {
          gfx::Image& image =
              ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                  prepopulated_page.favicon_id);
          ReturnRenderedIconForRequest(request, image.AsBitmap());
          return;
        }
      }
    }

    // This will query for a local favicon. If not found, will take alternative
    // action in OnLocalFaviconAvailable.
    const bool fallback_to_host = true;
    favicon_service->GetRawFaviconForPageURL(
        parsed.url, {favicon_base::IconType::kFavicon}, icon_size_in_pixels,
        fallback_to_host,
        base::Bind(&NtpIconSource::OnLocalFaviconAvailable,
                   weak_ptr_factory_.GetWeakPtr(), request),
        &cancelable_task_tracker_);
  } else {
    callback.Run(nullptr);
  }
}

std::string NtpIconSource::GetMimeType(const std::string&) const {
  // NOTE: this may not always be correct for all possible types that this
  // source will serve. Seems to work fine, however.
  return "image/png";
}

bool NtpIconSource::AllowCaching() const {
  return false;
}

bool NtpIconSource::ShouldServiceRequest(
    const GURL& url,
    content::ResourceContext* resource_context,
    int render_process_id) const {
  if (url.SchemeIs(chrome::kChromeSearchScheme)) {
    return InstantIOContext::ShouldServiceRequest(url, resource_context,
                                                  render_process_id);
  }
  return URLDataSource::ShouldServiceRequest(url, resource_context,
                                             render_process_id);
}

void NtpIconSource::OnLocalFaviconAvailable(
    const NtpIconRequest& request,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  if (bitmap_result.is_valid()) {
    // A local favicon was found. Decode it to an SkBitmap so it can eventually
    // be passed as valid image data to RenderIconBitmap.
    SkBitmap bitmap;
    bool result =
        gfx::PNGCodec::Decode(bitmap_result.bitmap_data.get()->front(),
                              bitmap_result.bitmap_data.get()->size(), &bitmap);
    DCHECK(result);
    ReturnRenderedIconForRequest(request, bitmap);
  } else {
    // Since a local favicon was not found, attempt to fetch a server icon if
    // the url is known to the server (this last check is important to avoid
    // leaking private history to the server).
    RequestServerFavicon(request);
  }
}

bool NtpIconSource::IsRequestedUrlInServerSuggestions(const GURL& url) {
  suggestions::SuggestionsService* suggestions_service =
      suggestions::SuggestionsServiceFactory::GetForProfile(profile_);
  if (!suggestions_service)
    return false;

  suggestions::SuggestionsProfile profile =
      suggestions_service->GetSuggestionsDataFromCache().value_or(
          suggestions::SuggestionsProfile());
  auto position =
      std::find_if(profile.suggestions().begin(), profile.suggestions().end(),
                   [url](const suggestions::ChromeSuggestion& suggestion) {
                     return suggestion.url() == url.spec();
                   });
  return position != profile.suggestions().end();
}

void NtpIconSource::RequestServerFavicon(const NtpIconRequest& request) {
  // Only fetch a server icon if the page url is known to the server. This check
  // is important to avoid leaking private history to the server.
  const GURL server_favicon_url =
      GURL(base::StringPrintf(kServerFaviconURL, request.path.spec().c_str()));
  if (!server_favicon_url.is_valid() ||
      !IsRequestedUrlInServerSuggestions(request.path)) {
    ReturnRenderedIconForRequest(request, SkBitmap());
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ntp_icon_source", R"(
      semantics {
        sender: "NTP Icon Source"
        description:
          "Retrieves icons for site suggestions based on the user's browsing "
          "history, for use e.g. on the New Tab page."
        trigger:
          "Triggered when an icon for a suggestion is required (e.g. on "
          "the New Tab page), no local icon is available and the URL is known "
          "to the server (hence no private information is revealed)."
        data: "The URL for which to retrieve an icon."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users cannot disable this feature. The feature is enabled by "
          "default."
        policy_exception_justification: "Not implemented."
      })");
  image_fetcher_->SetDesiredImageFrameSize(
      gfx::Size(request.icon_size_in_pixels, request.icon_size_in_pixels));
  image_fetcher_->FetchImage(
      /*id=*/std::string(), server_favicon_url,
      base::Bind(&NtpIconSource::OnServerFaviconAvailable,
                 weak_ptr_factory_.GetWeakPtr(), request),
      traffic_annotation);
}

void NtpIconSource::OnServerFaviconAvailable(
    const NtpIconRequest& request,
    const std::string& id,
    const gfx::Image& fetched_image,
    const image_fetcher::RequestMetadata& metadata) {
  // If a server icon was not found, |fetched_bitmap| will be empty and a
  // fallback icon will be eventually drawn.
  SkBitmap fetched_bitmap = fetched_image.AsBitmap();
  if (!fetched_bitmap.empty()) {
    // The received server icon bitmap may still be bigger than our desired
    // size, so resize it.
    fetched_bitmap = skia::ImageOperations::Resize(
        fetched_bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
        request.icon_size_in_pixels, request.icon_size_in_pixels);
  }

  ReturnRenderedIconForRequest(request, fetched_bitmap);
}

void NtpIconSource::ReturnRenderedIconForRequest(const NtpIconRequest& request,
                                                 const SkBitmap& bitmap) {
  int desired_overall_size_in_pixel =
      std::ceil(kFallbackIconSizeDip * request.device_scale_factor);
  std::vector<unsigned char> bitmap_data =
      RenderIconBitmap(request.path, bitmap, desired_overall_size_in_pixel);
  request.callback.Run(base::RefCountedBytes::TakeVector(&bitmap_data));
}
