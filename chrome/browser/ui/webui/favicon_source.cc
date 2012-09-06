// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/favicon_source.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "grit/locale_settings.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/favicon_size.h"

FaviconSource::FaviconSource(Profile* profile, IconType type)
    : DataSource(type == FAVICON ? chrome::kChromeUIFaviconHost :
                     chrome::kChromeUITouchIconHost,
                 MessageLoop::current()) {
  Init(profile, type);
}

FaviconSource::FaviconSource(Profile* profile,
                             IconType type,
                             const std::string& source_name)
    : DataSource(source_name, MessageLoop::current()) {
  Init(profile, type);
}

FaviconSource::~FaviconSource() {
}

void FaviconSource::Init(Profile* profile, IconType type) {
  profile_ = profile->GetOriginalProfile();
  icon_types_ = type == FAVICON ? history::FAVICON :
      history::TOUCH_PRECOMPOSED_ICON | history::TOUCH_ICON |
      history::FAVICON;
}

void FaviconSource::StartDataRequest(const std::string& path,
                                     bool is_incognito,
                                     int request_id) {
  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (!favicon_service || path.empty()) {
    SendDefaultResponse(request_id);
    return;
  }

  FaviconService::Handle handle;
  if (path.size() > 8 && path.substr(0, 8) == "iconurl/") {
    // TODO : Change GetFavicon to support combination of IconType.
    handle = favicon_service->GetRawFavicon(
        GURL(path.substr(8)),
        history::FAVICON,
        gfx::kFaviconSize,
        ui::SCALE_FACTOR_100P,
        &cancelable_consumer_,
        base::Bind(&FaviconSource::OnFaviconDataAvailable,
                   base::Unretained(this)));
  } else {
    GURL url;

    if (path.size() > 5 && path.substr(0, 5) == "size/") {
      size_t slash = path.find("/", 5);
      std::string size = path.substr(5, slash - 5);
      int pixel_size = atoi(size.c_str());
      CHECK(pixel_size == 32 || pixel_size == 16) <<
          "only 32x32 and 16x16 icons are supported";
      request_size_map_[request_id] = pixel_size;
      url = GURL(path.substr(slash + 1));
    } else {
      // URL requests prefixed with "origin/" are converted to a form with an
      // empty path and a valid scheme. (e.g., example.com -->
      // http://example.com/ or http://example.com/a --> http://example.com/)
      if (path.size() > 7 && path.substr(0, 7) == "origin/") {
        std::string originalUrl = path.substr(7);

        // If the original URL does not specify a scheme (e.g., example.com
        // instead of http://example.com), add "http://" as a default.
        if (!GURL(originalUrl).has_scheme())
          originalUrl = "http://" + originalUrl;

        // Strip the path beyond the top-level domain.
        url = GURL(originalUrl).GetOrigin();
      } else {
        url = GURL(path);
      }

      request_size_map_[request_id] = 16;
    }

    // Intercept requests for prepopulated pages.
    for (size_t i = 0; i < arraysize(history::kPrepopulatedPages); i++) {
      if (url.spec() ==
          l10n_util::GetStringUTF8(history::kPrepopulatedPages[i].url_id)) {
        request_size_map_.erase(request_id);
        SendResponse(request_id,
            ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
                history::kPrepopulatedPages[i].favicon_id,
                ui::SCALE_FACTOR_100P));
        return;
      }
    }

    // TODO(estade): fetch the requested size.
    handle = favicon_service->GetRawFaviconForURL(
        FaviconService::FaviconForURLParams(
            profile_,
            url,
            icon_types_,
            gfx::kFaviconSize,
            &cancelable_consumer_),
        ui::SCALE_FACTOR_100P,
        base::Bind(&FaviconSource::OnFaviconDataAvailable,
                   base::Unretained(this)));
  }

  // Attach the ChromeURLDataManager request ID to the history request.
  cancelable_consumer_.SetClientData(favicon_service, handle, request_id);
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

void FaviconSource::OnFaviconDataAvailable(
    FaviconService::Handle request_handle,
    const history::FaviconBitmapResult& bitmap_result) {
  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  int request_id = cancelable_consumer_.GetClientData(favicon_service,
                                                      request_handle);

  if (bitmap_result.is_valid()) {
    // Forward the data along to the networking system.
    SendResponse(request_id, bitmap_result.bitmap_data);
  } else {
    SendDefaultResponse(request_id);
  }
}

void FaviconSource::SendDefaultResponse(int request_id) {
  base::RefCountedMemory* bytes = NULL;
  if (request_size_map_[request_id] == 32) {
    if (!default_favicon_large_.get()) {
      default_favicon_large_ =
          ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              IDR_DEFAULT_LARGE_FAVICON, ui::SCALE_FACTOR_100P);
    }
    bytes = default_favicon_large_;
  } else {
    if (!default_favicon_.get()) {
      default_favicon_ =
          ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              IDR_DEFAULT_FAVICON, ui::SCALE_FACTOR_100P);
    }
    bytes = default_favicon_;
  }
  request_size_map_.erase(request_id);

  SendResponse(request_id, bytes);
}
