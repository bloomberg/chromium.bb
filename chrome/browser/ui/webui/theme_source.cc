// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/theme_source.h"

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resources_util.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/common/url_constants.h"
#include "net/url_request/url_request.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "url/gurl.h"

namespace {

GURL GetThemeUrl(const std::string& path) {
  return GURL(std::string(content::kChromeUIScheme) + "://" +
              std::string(chrome::kChromeUIThemeHost) + "/" + path);
}

bool IsNewTabCssPath(const std::string& path) {
  static const char kNewTabThemeCssPath[] = "css/new_tab_theme.css";
  static const char kIncognitoTabThemeCssPath[] = "css/incognito_tab_theme.css";
  return path == kNewTabThemeCssPath || path == kIncognitoTabThemeCssPath;
}

void ProcessImageOnUiThread(const gfx::ImageSkia& image,
                            float scale,
                            scoped_refptr<base::RefCountedBytes> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const gfx::ImageSkiaRep& rep = image.GetRepresentation(scale);
  gfx::PNGCodec::EncodeBGRASkBitmap(
      rep.GetBitmap(), false /* discard transparency */, &data->data());
}

void ProcessResourceOnUiThread(int resource_id,
                               float scale,
                               scoped_refptr<base::RefCountedBytes> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ProcessImageOnUiThread(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id),
      scale, data);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ThemeSource, public:

ThemeSource::ThemeSource(Profile* profile)
    : profile_(profile), serve_untrusted_(false) {}

ThemeSource::ThemeSource(Profile* profile, bool serve_untrusted)
    : profile_(profile), serve_untrusted_(serve_untrusted) {}

ThemeSource::~ThemeSource() = default;

std::string ThemeSource::GetSource() {
  return serve_untrusted_ ? chrome::kChromeUIUntrustedThemeURL
                          : chrome::kChromeUIThemeHost;
}

void ThemeSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  // TODO(crbug/1009127): Simplify usages of |path| since |url| is available.
  const std::string path = content::URLDataSource::URLToRequestPath(url);
  // Default scale factor if not specified.
  float scale = 1.0f;
  // All frames by default if not specified.
  int frame = -1;
  std::string parsed_path;
  webui::ParsePathAndImageSpec(GetThemeUrl(path), &parsed_path, &scale, &frame);

  if (IsNewTabCssPath(parsed_path)) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    NTPResourceCache::WindowType type =
        NTPResourceCache::GetWindowType(profile_, /*render_host=*/nullptr);
    NTPResourceCache* cache = NTPResourceCacheFactory::GetForProfile(profile_);
    std::move(callback).Run(cache->GetNewTabCSS(type));
    return;
  }

  int resource_id = -1;
  if (parsed_path == "current-channel-logo") {
    switch (chrome::GetChannel()) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      case version_info::Channel::CANARY:
        resource_id = IDR_PRODUCT_LOGO_32_CANARY;
        break;
      case version_info::Channel::DEV:
        resource_id = IDR_PRODUCT_LOGO_32_DEV;
        break;
      case version_info::Channel::BETA:
        resource_id = IDR_PRODUCT_LOGO_32_BETA;
        break;
      case version_info::Channel::STABLE:
        resource_id = IDR_PRODUCT_LOGO_32;
        break;
#else
      case version_info::Channel::CANARY:
      case version_info::Channel::DEV:
      case version_info::Channel::BETA:
      case version_info::Channel::STABLE:
        NOTREACHED();
        FALLTHROUGH;
#endif
      case version_info::Channel::UNKNOWN:
        resource_id = IDR_PRODUCT_LOGO_32;
        break;
    }
  } else {
    resource_id = ResourcesUtil::GetThemeResourceId(parsed_path);
  }

  // Limit the maximum scale we'll respond to.  Very large scale factors can
  // take significant time to serve or, at worst, crash the browser due to OOM.
  // We don't want to clamp to the max scale factor, though, for devices that
  // use 2x scale without 2x data packs, as well as omnibox requests for larger
  // (but still reasonable) scales (see below).
  const float max_scale = ui::GetScaleForScaleFactor(
      ui::ResourceBundle::GetSharedInstance().GetMaxScaleFactor());
  const float unreasonable_scale = max_scale * 32;
  // TODO(reveman): Add support frames beyond 0 (crbug.com/750064).
  if ((resource_id == -1) || (scale >= unreasonable_scale) || (frame > 0)) {
    // Either we have no data to send back, or the requested scale is
    // unreasonably large.  This shouldn't happen normally, as chrome://theme/
    // URLs are only used by WebUI pages and component extensions.  However, the
    // user can also enter these into the omnibox, so we need to fail
    // gracefully.
    std::move(callback).Run(nullptr);
  } else if ((GetMimeType(path) == "image/png") &&
             ((scale > max_scale) || (frame != -1))) {
    // This will extract and scale frame 0 of animated images.
    // TODO(reveman): Support scaling of animated images and avoid scaling and
    // re-encode when specific frame is specified (crbug.com/750064).
    DCHECK_LE(frame, 0);
    SendThemeImage(std::move(callback), resource_id, scale);
  } else {
    SendThemeBitmap(std::move(callback), resource_id, scale);
  }
}

std::string ThemeSource::GetMimeType(const std::string& path) {
  std::string parsed_path;
  webui::ParsePathAndScale(GetThemeUrl(path), &parsed_path, nullptr);
  return IsNewTabCssPath(parsed_path) ? "text/css" : "image/png";
}

bool ThemeSource::AllowCaching() {
  return false;
}

bool ThemeSource::ShouldServiceRequest(const GURL& url,
                                       content::BrowserContext* browser_context,
                                       int render_process_id) {
  return url.SchemeIs(chrome::kChromeSearchScheme)
             ? InstantService::ShouldServiceRequest(url, browser_context,
                                                    render_process_id)
             : URLDataSource::ShouldServiceRequest(url, browser_context,
                                                   render_process_id);
}

////////////////////////////////////////////////////////////////////////////////
// ThemeSource, private:

void ThemeSource::SendThemeBitmap(
    content::URLDataSource::GotDataCallback callback,
    int resource_id,
    float scale) {
  ui::ScaleFactor scale_factor = ui::GetSupportedScaleFactor(scale);
  if (BrowserThemePack::IsPersistentImageID(resource_id)) {
    scoped_refptr<base::RefCountedMemory> image_data(
        ThemeService::GetThemeProviderForProfile(profile_->GetOriginalProfile())
            .GetRawData(resource_id, scale_factor));
    std::move(callback).Run(image_data.get());
  } else {
    const ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    std::move(callback).Run(
        rb.LoadDataResourceBytesForScale(resource_id, scale_factor));
  }
}

void ThemeSource::SendThemeImage(
    content::URLDataSource::GotDataCallback callback,
    int resource_id,
    float scale) {
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  if (BrowserThemePack::IsPersistentImageID(resource_id)) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    const ui::ThemeProvider& tp = ThemeService::GetThemeProviderForProfile(
        profile_->GetOriginalProfile());
    ProcessImageOnUiThread(*tp.GetImageSkiaNamed(resource_id), scale, data);
    std::move(callback).Run(data.get());
  } else {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    // Fetching image data in ResourceBundle should happen on the UI thread. See
    // crbug.com/449277
    base::PostTaskAndReply(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ProcessResourceOnUiThread, resource_id, scale, data),
        base::BindOnce(std::move(callback), data));
  }
}

std::string ThemeSource::GetAccessControlAllowOriginForOrigin(
    const std::string& origin) {
  std::string allowed_origin_prefix = content::kChromeUIScheme;
  allowed_origin_prefix += "://";
  if (base::StartsWith(origin, allowed_origin_prefix,
                       base::CompareCase::SENSITIVE)) {
    return origin;
  }

  return content::URLDataSource::GetAccessControlAllowOriginForOrigin(origin);
}
