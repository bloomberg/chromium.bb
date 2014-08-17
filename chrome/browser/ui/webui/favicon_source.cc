// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/favicon_source.h"

#include <cmath>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/sync/open_tabs_ui_delegate.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/favicon/favicon_url_parser.h"
#include "chrome/common/url_constants.h"
#include "grit/locale_settings.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/ui_resources.h"

FaviconSource::IconRequest::IconRequest()
    : size_in_dip(gfx::kFaviconSize), device_scale_factor(1.0f) {
}

FaviconSource::IconRequest::IconRequest(
    const content::URLDataSource::GotDataCallback& cb,
    const GURL& path,
    int size,
    float scale)
    : callback(cb),
      request_path(path),
      size_in_dip(size),
      device_scale_factor(scale) {
}

FaviconSource::IconRequest::~IconRequest() {
}

FaviconSource::FaviconSource(Profile* profile, IconType type)
    : profile_(profile->GetOriginalProfile()),
      icon_types_(type == FAVICON ? favicon_base::FAVICON
                                  : favicon_base::TOUCH_PRECOMPOSED_ICON |
                                        favicon_base::TOUCH_ICON |
                                        favicon_base::FAVICON) {}

FaviconSource::~FaviconSource() {
}

std::string FaviconSource::GetSource() const {
  return icon_types_ == favicon_base::FAVICON ? chrome::kChromeUIFaviconHost
                                              : chrome::kChromeUITouchIconHost;
}

void FaviconSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (!favicon_service) {
    SendDefaultResponse(callback);
    return;
  }

  chrome::ParsedFaviconPath parsed;
  bool success = chrome::ParseFaviconPath(path, icon_types_, &parsed);
  if (!success) {
    SendDefaultResponse(callback);
    return;
  }

  GURL url(parsed.url);
  int desired_size_in_pixel =
      std::ceil(parsed.size_in_dip * parsed.device_scale_factor);

  if (parsed.is_icon_url) {
    // TODO(michaelbai): Change GetRawFavicon to support combination of
    // IconType.
   favicon_service->GetRawFavicon(
        url,
        favicon_base::FAVICON,
        desired_size_in_pixel,
        base::Bind(
            &FaviconSource::OnFaviconDataAvailable,
            base::Unretained(this),
            IconRequest(
                callback, url, parsed.size_in_dip, parsed.device_scale_factor)),
        &cancelable_task_tracker_);
  } else {
    // Intercept requests for prepopulated pages.
    for (size_t i = 0; i < arraysize(history::kPrepopulatedPages); i++) {
      if (url.spec() ==
          l10n_util::GetStringUTF8(history::kPrepopulatedPages[i].url_id)) {
        ui::ScaleFactor resource_scale_factor =
            ui::GetSupportedScaleFactor(parsed.device_scale_factor);
        callback.Run(
            ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
                history::kPrepopulatedPages[i].favicon_id,
                resource_scale_factor));
        return;
      }
    }

    favicon_service->GetRawFaviconForPageURL(
        url,
        icon_types_,
        desired_size_in_pixel,
        base::Bind(
            &FaviconSource::OnFaviconDataAvailable,
            base::Unretained(this),
            IconRequest(
                callback, url, parsed.size_in_dip, parsed.device_scale_factor)),
        &cancelable_task_tracker_);
  }
}

std::string FaviconSource::GetMimeType(const std::string&) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

bool FaviconSource::ShouldReplaceExistingSource() const {
  // Leave the existing DataSource in place, otherwise we'll drop any pending
  // requests on the floor.
  return false;
}

bool FaviconSource::ShouldServiceRequest(const net::URLRequest* request) const {
  if (request->url().SchemeIs(chrome::kChromeSearchScheme))
    return InstantIOContext::ShouldServiceRequest(request);
  return URLDataSource::ShouldServiceRequest(request);
}

bool FaviconSource::HandleMissingResource(const IconRequest& request) {
  // If the favicon is not available, try to use the synced favicon.
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  browser_sync::OpenTabsUIDelegate* open_tabs = sync_service ?
      sync_service->GetOpenTabsUIDelegate() : NULL;

  scoped_refptr<base::RefCountedMemory> response;
  if (open_tabs &&
      open_tabs->GetSyncedFaviconForPageURL(request.request_path.spec(),
                                            &response)) {
    request.callback.Run(response.get());
    return true;
  }
  return false;
}

void FaviconSource::OnFaviconDataAvailable(
    const IconRequest& request,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  if (bitmap_result.is_valid()) {
    // Forward the data along to the networking system.
    request.callback.Run(bitmap_result.bitmap_data.get());
  } else if (!HandleMissingResource(request)) {
    SendDefaultResponse(request);
  }
}

void FaviconSource::SendDefaultResponse(
    const content::URLDataSource::GotDataCallback& callback) {
  SendDefaultResponse(IconRequest(callback, GURL(), 16, 1.0f));
}

void FaviconSource::SendDefaultResponse(const IconRequest& icon_request) {
  int favicon_index;
  int resource_id;
  switch (icon_request.size_in_dip) {
    case 64:
      favicon_index = SIZE_64;
      resource_id = IDR_DEFAULT_FAVICON_64;
      break;
    case 32:
      favicon_index = SIZE_32;
      resource_id = IDR_DEFAULT_FAVICON_32;
      break;
    default:
      favicon_index = SIZE_16;
      resource_id = IDR_DEFAULT_FAVICON;
      break;
  }
  base::RefCountedMemory* default_favicon =
      default_favicons_[favicon_index].get();

  if (!default_favicon) {
    ui::ScaleFactor resource_scale_factor =
        ui::GetSupportedScaleFactor(icon_request.device_scale_factor);
    default_favicon =
        ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
            resource_id, resource_scale_factor);
    default_favicons_[favicon_index] = default_favicon;
  }

  icon_request.callback.Run(default_favicon);
}
