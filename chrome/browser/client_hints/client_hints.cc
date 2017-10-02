// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/client_hints/client_hints.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/client_hints/client_hints.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/origin_util.h"
#include "net/http/http_request_headers.h"
#include "third_party/WebKit/common/client_hints/client_hints.h"
#include "third_party/WebKit/common/device_memory/approximated_device_memory.h"
#include "third_party/WebKit/public/platform/WebClientHintsType.h"
#include "url/gurl.h"

namespace client_hints {

std::unique_ptr<net::HttpRequestHeaders>
GetAdditionalNavigationRequestClientHintsHeaders(
    content::BrowserContext* context,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Get the client hint headers.
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme))
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile)
    return nullptr;

  ContentSettingsForOneType client_hints_host_settings;
  HostContentSettingsMapFactory::GetForProfile(profile)->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
      &client_hints_host_settings);

  blink::WebEnabledClientHints web_client_hints;

  GetAllowedClientHintsFromSource(url, client_hints_host_settings,
                                  &web_client_hints);

  std::unique_ptr<net::HttpRequestHeaders> additional_headers(
      base::MakeUnique<net::HttpRequestHeaders>());

  // Currently, only "device-memory" client hint request header is added from
  // the browser process.
  if (web_client_hints.IsEnabled(
          blink::mojom::WebClientHintsType::kDeviceMemory)) {
    additional_headers->SetHeader(
        blink::kClientHintsHeaderMapping[static_cast<int>(
            blink::mojom::WebClientHintsType::kDeviceMemory)],
        base::DoubleToString(
            blink::ApproximatedDeviceMemory::GetApproximatedDeviceMemory()));
  }

  // Static assert that triggers if a new client hint header is added. If a new
  // client hint header is added, the following assertion should be updated.
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

}  // namespace client_hints
