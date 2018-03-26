// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "chrome/browser/client_hints/client_hints.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/client_hints/client_hints.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/origin_util.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "third_party/WebKit/public/common/client_hints/client_hints.h"
#include "third_party/WebKit/public/common/device_memory/approximated_device_memory.h"
#include "third_party/WebKit/public/platform/WebClientHintsType.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include "content/public/browser/host_zoom_map.h"
#include "content/public/common/page_zoom.h"
#endif  // !OS_ANDROID

namespace {

bool IsJavaScriptAllowed(Profile* profile, const GURL& url) {
  return HostContentSettingsMapFactory::GetForProfile(profile)
             ->GetContentSetting(url, url, CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                 std::string()) == CONTENT_SETTING_ALLOW;
}

// Returns the zoom factor for a given |url|.
double GetZoomFactor(content::BrowserContext* context, const GURL& url) {
// Android does not have the concept of zooming in like desktop.
#if defined(OS_ANDROID)
  return 1.0;
#else

  double zoom_level = content::HostZoomMap::GetDefaultForBrowserContext(context)
                          ->GetZoomLevelForHostAndScheme(
                              url.scheme(), net::GetHostOrSpecFromURL(url));

  if (zoom_level == 0.0) {
    // Get default zoom level.
    zoom_level = content::HostZoomMap::GetDefaultForBrowserContext(context)
                     ->GetDefaultZoomLevel();
  }

  return content::ZoomLevelToZoomFactor(zoom_level);
#endif
}

double GetDeviceScaleFactor() {
  double device_scale_factor = 1.0;
  if (display::Screen::GetScreen()) {
    device_scale_factor =
        display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();
  }
  DCHECK_LT(0.0, device_scale_factor);
  return device_scale_factor;
}

}  // namespace

namespace client_hints {

std::unique_ptr<net::HttpRequestHeaders>
GetAdditionalNavigationRequestClientHintsHeaders(
    content::BrowserContext* context,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Get the client hint headers.
  if (!url.is_valid())
    return nullptr;

  if (!url.SchemeIsHTTPOrHTTPS())
    return nullptr;

  if (url.SchemeIs(url::kHttpScheme) && !net::IsLocalhost(url))
    return nullptr;

  DCHECK(url.SchemeIs(url::kHttpsScheme) ||
         (url.SchemeIs(url::kHttpScheme) && net::IsLocalhost(url)));

  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile)
    return nullptr;

  // Check if |url| is allowed to run JavaScript. If not, client hints are not
  // attached to the requests that initiate on the browser side.
  if (!IsJavaScriptAllowed(profile, url)) {
    return nullptr;
  }

  ContentSettingsForOneType client_hints_host_settings;
  HostContentSettingsMapFactory::GetForProfile(profile)->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
      &client_hints_host_settings);

  blink::WebEnabledClientHints web_client_hints;

  GetAllowedClientHintsFromSource(
      url /* resource url */, client_hints_host_settings, &web_client_hints);

  std::unique_ptr<net::HttpRequestHeaders> additional_headers(
      std::make_unique<net::HttpRequestHeaders>());

  // Currently, only "device-memory" client hint request header is added from
  // the browser process.
  if (web_client_hints.IsEnabled(
          blink::mojom::WebClientHintsType::kDeviceMemory)) {
    // Initialize device memory if it's not already.
    blink::ApproximatedDeviceMemory::Initialize();
    const float device_memory =
        blink::ApproximatedDeviceMemory::GetApproximatedDeviceMemory();
    DCHECK_LT(0.0, device_memory);
    additional_headers->SetHeader(
        blink::kClientHintsHeaderMapping[static_cast<int>(
            blink::mojom::WebClientHintsType::kDeviceMemory)],
        base::NumberToString(device_memory));
  }

  if (web_client_hints.IsEnabled(blink::mojom::WebClientHintsType::kDpr)) {
    double device_scale_factor = GetDeviceScaleFactor();

    double zoom_factor = GetZoomFactor(context, url);
    additional_headers->SetHeader(
        blink::kClientHintsHeaderMapping[static_cast<int>(
            blink::mojom::WebClientHintsType::kDpr)],
        base::NumberToString(device_scale_factor * zoom_factor));
  }

  if (web_client_hints.IsEnabled(
          blink::mojom::WebClientHintsType::kViewportWidth)) {
    // The default value on Android. See
    // https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/css/viewportAndroid.css.
    double viewport_width = 980;

#if !defined(OS_ANDROID)
    double device_scale_factor = GetDeviceScaleFactor();
    viewport_width = (display::Screen::GetScreen()
                          ->GetPrimaryDisplay()
                          .GetSizeInPixel()
                          .width()) /
                     GetZoomFactor(context, url) / device_scale_factor;
#endif  // !OS_ANDROID
    DCHECK_LT(0, viewport_width);
    if (viewport_width > 0) {
      additional_headers->SetHeader(
          blink::kClientHintsHeaderMapping[static_cast<int>(
              blink::mojom::WebClientHintsType::kViewportWidth)],
          base::NumberToString(std::round(viewport_width)));
    }
  }

  // Static assert that triggers if a new client hint header is added. If a
  // new client hint header is added, the following assertion should be updated.
  // If possible, logic should be added above so that the request headers for
  // the newly added client hint can be added to the request.
  static_assert(
      blink::mojom::WebClientHintsType::kViewportWidth ==
          blink::mojom::WebClientHintsType::kLast,
      "Consider adding client hint request headers from the browser process");

  // TODO(crbug.com/735518): If the request is redirected, the client hint
  // headers stay attached to the redirected request. Consider removing/adding
  // the client hints headers if the request is redirected with a change in
  // scheme or a change in the origin.
  return additional_headers;
}

void RequestBeginning(
    net::URLRequest* request,
    scoped_refptr<content_settings::CookieSettings> cookie_settings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!cookie_settings)
    return;

  if (cookie_settings->IsCookieAccessAllowed(request->url(),
                                             request->site_for_cookies())) {
    return;
  }

  // If |primary_url| is disallowed from storing cookies, then client hints are
  // not attached to the requests sent to |primary_url|.
  for (size_t i = 0; i < blink::kClientHintsHeaderMappingCount; ++i)
    request->RemoveRequestHeaderByName(blink::kClientHintsHeaderMapping[i]);
}

}  // namespace client_hints
