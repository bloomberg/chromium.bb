// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/ntp_icon_source.h"

#include <stddef.h>
#include <algorithm>
#include <cmath>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
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
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"

namespace {

const char kImageFetcherUmaClientName[] = "NtpIconSource";

// The color of the letter drawn for a fallback icon.  Changing this may require
// changing the algorithm in RenderIconBitmap() that guarantees contrast.
constexpr SkColor kFallbackIconLetterColor = SK_ColorWHITE;

// Delimiter in the url that looks for the size specification.
const char kSizeParameter[] = "size/";

// Delimiter in the url for dark mode specification.
const char kDarkModeParameter[] = "dark/";

// Size of the icon background (gray circle), in dp.
const int kIconSizeDip = 48;

// Size of the favicon fallback (letter + colored circle), in dp.
const int kFallbackSizeDip = 32;

// URL to the server favicon service. "alt=404" means the service will return a
// 404 if an icon can't be found.
const char kServerFaviconURL[] =
    "https://s2.googleusercontent.com/s2/favicons?domain_url=%s&alt=404&sz=32";

// URL for the site's icon source.
const char kSiteFaviconURL[] = "%sfavicon.ico";

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

  // Parse the dark mode spec (e.g. "dark"), if available. The value is not
  // used, but is required to generate a new icon for dark mode.
  if (HasSubstringAt(path, parsed_index, kDarkModeParameter)) {
    slash = path.find("/", parsed_index);
    if (slash == std::string::npos)
      return parsed;
    parsed_index = slash + 1;
  }

  parsed.url = GURL(path.substr(parsed_index));
  return parsed;
}

// Draws a circle of a given |size| and |offset| in the |canvas| and fills it
// with |background_color|.
void DrawCircleInCanvas(gfx::Canvas* canvas,
                        int size,
                        int offset,
                        SkColor background_color) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(background_color);
  int corner_radius = static_cast<int>(size * 0.5 + 0.5);
  canvas->DrawRoundRect(gfx::Rect(offset, offset, size, size), corner_radius,
                        flags);
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

  const double kDefaultFontSizeRatio = 0.34;
  int font_size = static_cast<int>(size * kDefaultFontSizeRatio);
  if (font_size <= 0)
    return;

  gfx::Font::Weight font_weight = gfx::Font::Weight::NORMAL;

#if defined(OS_WIN)
  font_weight = gfx::Font::Weight::SEMIBOLD;
#endif

  // TODO(crbug.com/853780): Adjust the text color according to the background
  // color.
  canvas->DrawStringRectWithFlags(
      icon_text,
      gfx::FontList({l10n_util::GetStringUTF8(IDS_NTP_FONT_FAMILY)},
                    gfx::Font::NORMAL, font_size, font_weight),
      kFallbackIconLetterColor, gfx::Rect(0, 0, size, size),
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

// For the given |icon_url|, will render |favicon| within a gray, circular
// background. If |favicon| is not specifed, will use a colored circular
// monogram instead.
std::vector<unsigned char> RenderIconBitmap(const GURL& icon_url,
                                            const SkBitmap& favicon,
                                            int icon_size,
                                            int fallback_size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(icon_size, icon_size, false);
  cc::SkiaPaintCanvas paint_canvas(bitmap);
  gfx::Canvas canvas(&paint_canvas, 1.f);
  canvas.DrawColor(SK_ColorTRANSPARENT, SkBlendMode::kSrc);

  // Draw the gray background.
  SkColor favicon_bg =
      ui::NativeTheme::GetInstanceForNativeUi()->SystemDarkModeEnabled()
          ? gfx::kGoogleGrey900
          : gfx::kGoogleGrey100;
  DrawCircleInCanvas(&canvas, icon_size, /*offset=*/0,
                     /*background_color=*/favicon_bg);
  DrawFavicon(favicon, &canvas, icon_size);

  // If necessary, draw the colored fallback monogram.
  if (favicon.empty()) {
    SkColor fallback_color = color_utils::GetColorWithMinimumContrast(
        GetBackgroundColorForUrl(icon_url), kFallbackIconLetterColor);

    int offset = (icon_size - fallback_size) / 2;
    DrawCircleInCanvas(&canvas, fallback_size, offset, fallback_color);
    DrawFallbackIconLetter(icon_url, icon_size, &canvas);
  }

  std::vector<unsigned char> bitmap_data;
  bool result = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &bitmap_data);
  DCHECK(result);
  return bitmap_data;
}

SkBitmap GetBitmapAndResize(const gfx::Image& fetched_image, int desired_size) {
  SkBitmap fetched_bitmap = fetched_image.AsBitmap();
  if (!fetched_bitmap.empty()) {
    // The resize the icon bitmap as necessary.
    fetched_bitmap = skia::ImageOperations::Resize(
        fetched_bitmap, skia::ImageOperations::RESIZE_LANCZOS3, desired_size,
        desired_size);
  }
  return fetched_bitmap;
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
      weak_ptr_factory_(this) {}

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

          // Resize as necessary.
          gfx::Size target_size(icon_size_in_pixels, icon_size_in_pixels);
          if (!image.IsEmpty() && image.Size() != target_size) {
            gfx::ImageSkia resized_image =
                gfx::ImageSkiaOperations::CreateResizedImage(
                    image.AsImageSkia(), skia::ImageOperations::RESIZE_BEST,
                    target_size);
            ReturnRenderedIconForRequest(request,
                                         gfx::Image(resized_image).AsBitmap());
          } else {
            ReturnRenderedIconForRequest(request, image.AsBitmap());
          }
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
  // is important to avoid leaking private history to the server. If the server
  // icon is not valid or known, request the site's favicon.ico directly.
  const GURL server_favicon_url =
      GURL(base::StringPrintf(kServerFaviconURL, request.path.spec().c_str()));
  if (!server_favicon_url.is_valid() ||
      !IsRequestedUrlInServerSuggestions(request.path)) {
    RequestSiteFavicon(request);
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
  StartFaviconRequest(request, server_favicon_url, traffic_annotation,
                      base::BindOnce(&NtpIconSource::OnServerFaviconAvailable,
                                     weak_ptr_factory_.GetWeakPtr(), request));
}

void NtpIconSource::RequestSiteFavicon(const NtpIconRequest& request) {
  // Only use the site's origin when requesting the favicon image.
  // e.g. www.foo.com/foo -> www.foo.com/favicon.ico
  const GURL favicon_url = GURL(base::StringPrintf(
      kSiteFaviconURL, request.path.GetOrigin().spec().c_str()));
  if (!favicon_url.is_valid()) {
    ReturnRenderedIconForRequest(request, SkBitmap());
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ntp_icon_source_site_favicon", R"(
      semantics {
        sender: "NTP Icon Source Site Favicon"
        description:
          "Retrieves icons for shortcuts on the New Tab Page directly from the "
          "site's favicon.ico image. These shortcuts can either be user "
          "created or taken from the user's browsing history."
        trigger:
          "Triggered when an icon for a shortcut on the New Tab Page is "
          "required, no local icon is available, and the favicon URL provided "
          "by the server suggestion (if available) does not resolve."
        data: "The URL for which to retrieve an icon."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users cannot disable this feature. The feature is enabled by "
          "default."
        policy_exception_justification: "Not implemented."
      })");
  StartFaviconRequest(request, favicon_url, traffic_annotation,
                      base::BindOnce(&NtpIconSource::OnSiteFaviconAvailable,
                                     weak_ptr_factory_.GetWeakPtr(), request));
}

void NtpIconSource::StartFaviconRequest(
    const NtpIconRequest& request,
    const GURL request_url,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    image_fetcher::ImageFetcherCallback callback) {
  if (!request_url.is_valid()) {
    ReturnRenderedIconForRequest(request, SkBitmap());
    return;
  }

  image_fetcher::ImageFetcherParams params(traffic_annotation,
                                           kImageFetcherUmaClientName);
  params.set_frame_size(
      gfx::Size(request.icon_size_in_pixels, request.icon_size_in_pixels));
  image_fetcher_->FetchImage(request_url, std::move(callback),
                             std::move(params));
}

void NtpIconSource::OnServerFaviconAvailable(
    const NtpIconRequest& request,
    const gfx::Image& fetched_image,
    const image_fetcher::RequestMetadata& metadata) {
  // If a server icon was not found, |fetched_bitmap| will be empty and another
  // request will be made for the site's favicon.ico image.
  SkBitmap fetched_bitmap =
      GetBitmapAndResize(fetched_image, request.icon_size_in_pixels);
  if (!fetched_bitmap.empty())
    ReturnRenderedIconForRequest(request, fetched_bitmap);
  else
    RequestSiteFavicon(request);
}

void NtpIconSource::OnSiteFaviconAvailable(
    const NtpIconRequest& request,
    const gfx::Image& fetched_image,
    const image_fetcher::RequestMetadata& metadata) {
  // If the icon was not found, |fetched_bitmap| will be empty and a fallback
  // icon will be eventually drawn.
  SkBitmap fetched_bitmap =
      GetBitmapAndResize(fetched_image, request.icon_size_in_pixels);
  ReturnRenderedIconForRequest(request, fetched_bitmap);
}

void NtpIconSource::ReturnRenderedIconForRequest(const NtpIconRequest& request,
                                                 const SkBitmap& bitmap) {
  // Only use even pixel sizes to avoid issues when centering the fallback
  // monogram.
  int desired_overall_size_in_pixel =
      std::round(kIconSizeDip * request.device_scale_factor * 0.5) * 2.0;
  int desired_fallback_size_in_pixel =
      std::round(kFallbackSizeDip * request.device_scale_factor * 0.5) * 2.0;
  std::vector<unsigned char> bitmap_data =
      RenderIconBitmap(request.path, bitmap, desired_overall_size_in_pixel,
                       desired_fallback_size_in_pixel);
  request.callback.Run(base::RefCountedBytes::TakeVector(&bitmap_data));
}
