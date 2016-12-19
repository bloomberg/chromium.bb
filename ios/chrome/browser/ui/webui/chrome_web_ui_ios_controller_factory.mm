// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/chrome_web_ui_ios_controller_factory.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/ui/webui/about_ui.h"
#include "ios/chrome/browser/ui/webui/crashes_ui.h"
#include "ios/chrome/browser/ui/webui/flags_ui.h"
#include "ios/chrome/browser/ui/webui/gcm/gcm_internals_ui.h"
#include "ios/chrome/browser/ui/webui/net_export/net_export_ui.h"
#include "ios/chrome/browser/ui/webui/ntp_tiles_internals_ui.h"
#include "ios/chrome/browser/ui/webui/omaha_ui.h"
#include "ios/chrome/browser/ui/webui/physical_web_ui.h"
#include "ios/chrome/browser/ui/webui/popular_sites_internals_ui.h"
#include "ios/chrome/browser/ui/webui/signin_internals_ui_ios.h"
#include "ios/chrome/browser/ui/webui/sync_internals/sync_internals_ui.h"
#include "ios/chrome/browser/ui/webui/version_ui.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

using web::WebUIIOS;
using web::WebUIIOSController;

namespace {

// A function for creating a new WebUIIOS. The caller owns the return value,
// which may be NULL.
typedef WebUIIOSController* (*WebUIIOSFactoryFunction)(WebUIIOS* web_ui,
                                                       const GURL& url);

// Template for defining WebUIIOSFactoryFunction.
template <class T>
WebUIIOSController* NewWebUIIOS(WebUIIOS* web_ui, const GURL& url) {
  return new T(web_ui);
}

template <class T>
WebUIIOSController* NewWebUIIOSWithHost(WebUIIOS* web_ui, const GURL& url) {
  return new T(web_ui, url.host());
}

// Returns a function that can be used to create the right type of WebUIIOS for
// a tab, based on its URL. Returns NULL if the URL doesn't have WebUIIOS
// associated with it.
WebUIIOSFactoryFunction GetWebUIIOSFactoryFunction(WebUIIOS* web_ui,
                                                   const GURL& url) {
  // This will get called a lot to check all URLs, so do a quick check of other
  // schemes to filter out most URLs.
  if (!url.SchemeIs(kChromeUIScheme))
    return nullptr;

  // Please keep this in alphabetical order. If #ifs or special logic is
  // required, add it below in the appropriate section.
  const std::string url_host = url.host();
  if (url_host == kChromeUIChromeURLsHost ||
      url_host == kChromeUIHistogramHost || url_host == kChromeUICreditsHost)
    return &NewWebUIIOSWithHost<AboutUI>;
  if (url_host == kChromeUICrashesHost)
    return &NewWebUIIOS<CrashesUI>;
  if (url_host == kChromeUIGCMInternalsHost)
    return &NewWebUIIOS<GCMInternalsUI>;
  if (url_host == kChromeUINetExportHost)
    return &NewWebUIIOS<NetExportUI>;
  if (url_host == kChromeUINTPTilesInternalsHost)
    return &NewWebUIIOS<NTPTilesInternalsUI>;
  if (url_host == kChromeUIOmahaHost)
    return &NewWebUIIOS<OmahaUI>;
  if (experimental_flags::IsPhysicalWebEnabled()) {
    if (url_host == kChromeUIPhysicalWebHost)
      return &NewWebUIIOS<PhysicalWebUI>;
  }
  if (url_host == kChromeUIPopularSitesInternalsHost)
    return &NewWebUIIOS<PopularSitesInternalsUI>;
  if (url_host == kChromeUISignInInternalsHost)
    return &NewWebUIIOS<SignInInternalsUIIOS>;
  if (url_host == kChromeUISyncInternalsHost)
    return &NewWebUIIOS<SyncInternalsUI>;
  if (url_host == kChromeUIVersionHost)
    return &NewWebUIIOS<VersionUI>;
  if (url_host == kChromeUIFlagsHost)
    return &NewWebUIIOS<FlagsUI>;
  if (url_host == kChromeUIAppleFlagsHost)
    return &NewWebUIIOS<AppleFlagsUI>;

  // NOTE: It's possible that |url| is a WebUI URL that will be handled by
  // ChromeWebUIControllerFactory. Once the iOS port is no longer using
  // ChromeWebUIControllerFactory, there should be a DLOG here noting that
  // |url| is an unknown WebUI URL.

  return nullptr;
}

void RunFaviconCallbackAsync(
    const favicon_base::FaviconResultsCallback& callback,
    const std::vector<favicon_base::FaviconRawBitmapResult>* results) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&favicon::FaviconService::FaviconResultsCallbackRunner,
                 callback, base::Owned(results)));
}

}  // namespace

WebUIIOSController*
ChromeWebUIIOSControllerFactory::CreateWebUIIOSControllerForURL(
    WebUIIOS* web_ui,
    const GURL& url) const {
  WebUIIOSFactoryFunction function = GetWebUIIOSFactoryFunction(web_ui, url);
  if (!function)
    return nullptr;

  return (*function)(web_ui, url);
}

// static
ChromeWebUIIOSControllerFactory*
ChromeWebUIIOSControllerFactory::GetInstance() {
  return base::Singleton<ChromeWebUIIOSControllerFactory>::get();
}

ChromeWebUIIOSControllerFactory::ChromeWebUIIOSControllerFactory() {}

ChromeWebUIIOSControllerFactory::~ChromeWebUIIOSControllerFactory() {}

void ChromeWebUIIOSControllerFactory::GetFaviconForURL(
    ios::ChromeBrowserState* browser_state,
    const GURL& page_url,
    const std::vector<int>& desired_sizes_in_pixel,
    const favicon_base::FaviconResultsCallback& callback) const {
  GURL url(page_url);

  std::vector<favicon_base::FaviconRawBitmapResult>* favicon_bitmap_results =
      new std::vector<favicon_base::FaviconRawBitmapResult>();

  // Use ui::GetSupportedScaleFactors instead of
  // favicon_base::GetFaviconScales() because chrome favicons comes from
  // resources.
  std::vector<ui::ScaleFactor> resource_scale_factors =
      ui::GetSupportedScaleFactors();

  std::vector<gfx::Size> candidate_sizes;
  for (size_t i = 0; i < resource_scale_factors.size(); ++i) {
    float scale = ui::GetScaleForScaleFactor(resource_scale_factors[i]);
    int candidate_edge_size =
        static_cast<int>(gfx::kFaviconSize * scale + 0.5f);
    candidate_sizes.push_back(
        gfx::Size(candidate_edge_size, candidate_edge_size));
  }
  std::vector<size_t> selected_indices;
  SelectFaviconFrameIndices(candidate_sizes, desired_sizes_in_pixel,
                            &selected_indices, nullptr);
  for (size_t i = 0; i < selected_indices.size(); ++i) {
    size_t selected_index = selected_indices[i];
    ui::ScaleFactor selected_resource_scale =
        resource_scale_factors[selected_index];

    scoped_refptr<base::RefCountedMemory> bitmap(
        GetFaviconResourceBytes(url, selected_resource_scale));
    if (bitmap.get() && bitmap->size()) {
      favicon_base::FaviconRawBitmapResult bitmap_result;
      bitmap_result.bitmap_data = bitmap;
      // Leave |bitmap_result|'s icon URL as the default of GURL().
      bitmap_result.icon_type = favicon_base::FAVICON;
      bitmap_result.pixel_size = candidate_sizes[selected_index];
      favicon_bitmap_results->push_back(bitmap_result);
    }
  }

  RunFaviconCallbackAsync(callback, favicon_bitmap_results);
}

base::RefCountedMemory*
ChromeWebUIIOSControllerFactory::GetFaviconResourceBytes(
    const GURL& page_url,
    ui::ScaleFactor scale_factor) const {
  if (!page_url.SchemeIs(kChromeUIScheme))
    return nullptr;

  if (page_url.host_piece() == kChromeUICrashesHost)
    return CrashesUI::GetFaviconResourceBytes(scale_factor);
  if (page_url.host_piece() == kChromeUIFlagsHost)
    return FlagsUI::GetFaviconResourceBytes(scale_factor);
  if (page_url.host_piece() == kChromeUIAppleFlagsHost)
    return AppleFlagsUI::GetFaviconResourceBytes(scale_factor);

  return nullptr;
}
