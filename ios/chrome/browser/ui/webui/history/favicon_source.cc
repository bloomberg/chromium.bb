// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/history/favicon_source.h"

#include <cmath>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/history/core/browser/top_sites.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/sync_driver/sync_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/ui_resources.h"

FaviconSource::IconRequest::IconRequest()
    : size_in_dip(gfx::kFaviconSize), device_scale_factor(1.0f) {}

FaviconSource::IconRequest::IconRequest(
    const web::URLDataSourceIOS::GotDataCallback& cb,
    const GURL& path,
    int size,
    float scale)
    : callback(cb),
      request_path(path),
      size_in_dip(size),
      device_scale_factor(scale) {}

FaviconSource::IconRequest::~IconRequest() {}

FaviconSource::FaviconSource(favicon::FaviconService* favicon_service,
                             const scoped_refptr<history::TopSites>& top_sites,
                             sync_driver::SyncService* sync_service)
    : favicon_service_(favicon_service),
      top_sites_(top_sites),
      sync_service_(sync_service) {}

FaviconSource::~FaviconSource() {}

std::string FaviconSource::GetSource() const {
  return std::string("touch-icon");
}

void FaviconSource::StartDataRequest(
    const std::string& path,
    const web::URLDataSourceIOS::GotDataCallback& callback) {
  if (!favicon_service_) {
    SendDefaultResponse(callback);
    return;
  }

  const int kIconTypes = favicon_base::TOUCH_PRECOMPOSED_ICON |
                         favicon_base::TOUCH_ICON | favicon_base::FAVICON;
  chrome::ParsedFaviconPath parsed;
  bool success = chrome::ParseFaviconPath(path, kIconTypes, &parsed);
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
    favicon_service_->GetRawFavicon(
        url, favicon_base::FAVICON, desired_size_in_pixel,
        base::Bind(&FaviconSource::OnFaviconDataAvailable,
                   base::Unretained(this),
                   IconRequest(callback, url, parsed.size_in_dip,
                               parsed.device_scale_factor)),
        &cancelable_task_tracker_);
  } else {
    // Intercept requests for prepopulated pages if TopSites exists.
    if (top_sites_) {
      for (const auto& prepopulated_page : top_sites_->GetPrepopulatedPages()) {
        if (url == prepopulated_page.most_visited.url) {
          ui::ScaleFactor resource_scale_factor =
              ui::GetSupportedScaleFactor(parsed.device_scale_factor);
          callback.Run(
              ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
                  prepopulated_page.favicon_id, resource_scale_factor));
          return;
        }
      }
    }

    favicon_service_->GetRawFaviconForPageURL(
        url, kIconTypes, desired_size_in_pixel,
        base::Bind(&FaviconSource::OnFaviconDataAvailable,
                   base::Unretained(this),
                   IconRequest(callback, url, parsed.size_in_dip,
                               parsed.device_scale_factor)),
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

bool FaviconSource::HandleMissingResource(const IconRequest& request) {
  // If the favicon is not available, try to use the synced favicon.
  sync_driver::OpenTabsUIDelegate* open_tabs =
      sync_service_ ? sync_service_->GetOpenTabsUIDelegate() : nullptr;

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
    const web::URLDataSourceIOS::GotDataCallback& callback) {
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
