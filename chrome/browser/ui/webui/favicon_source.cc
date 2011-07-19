// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/favicon_source.h"

#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"

FaviconSource::FaviconSource(Profile* profile, IconType type)
    : DataSource(type == FAVICON ? chrome::kChromeUIFaviconHost :
                     chrome::kChromeUITouchIconHost,
                 MessageLoop::current()),
      profile_(profile->GetOriginalProfile()),
      icon_types_(type == FAVICON ? history::FAVICON :
          history::TOUCH_PRECOMPOSED_ICON | history::TOUCH_ICON |
          history::FAVICON) {
}

FaviconSource::~FaviconSource() {
}

void FaviconSource::StartDataRequest(const std::string& path,
                                     bool is_incognito,
                                     int request_id) {
  FaviconService* favicon_service =
      profile_->GetFaviconService(Profile::EXPLICIT_ACCESS);
  if (favicon_service) {
    if (path.empty()) {
      SendDefaultResponse(request_id);
      return;
    }

    FaviconService::Handle handle;
    if (path.size() > 8 && path.substr(0, 8) == "iconurl/") {
      // TODO : Change GetFavicon to support combination of IconType.
      handle = favicon_service->GetFavicon(
          GURL(path.substr(8)),
          history::FAVICON,
          &cancelable_consumer_,
          NewCallback(this, &FaviconSource::OnFaviconDataAvailable));
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
        request_size_map_[request_id] = 16;
        url = GURL(path);
      }

      // TODO(estade): fetch the requested size.
      handle = favicon_service->GetFaviconForURL(
          url,
          icon_types_,
          &cancelable_consumer_,
          NewCallback(this, &FaviconSource::OnFaviconDataAvailable));
    }
    // Attach the ChromeURLDataManager request ID to the history request.
    cancelable_consumer_.SetClientData(favicon_service, handle, request_id);
  } else {
    SendResponse(request_id, NULL);
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

void FaviconSource::OnFaviconDataAvailable(
    FaviconService::Handle request_handle,
    history::FaviconData favicon) {
  FaviconService* favicon_service =
      profile_->GetFaviconService(Profile::EXPLICIT_ACCESS);
  int request_id = cancelable_consumer_.GetClientData(favicon_service,
                                                      request_handle);

  if (favicon.is_valid()) {
    // Forward the data along to the networking system.
    SendResponse(request_id, favicon.image_data);
  } else {
    SendDefaultResponse(request_id);
  }
}

void FaviconSource::SendDefaultResponse(int request_id) {
  RefCountedMemory* bytes = NULL;
  if (request_size_map_[request_id] == 32) {
    if (!default_favicon_large_.get()) {
      default_favicon_large_ =
          ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              IDR_DEFAULT_LARGE_FAVICON);
    }
    bytes = default_favicon_large_;
  } else {
    if (!default_favicon_.get()) {
      default_favicon_ =
          ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              IDR_DEFAULT_FAVICON);
    }
    bytes = default_favicon_;
  }
  request_size_map_.erase(request_id);

  SendResponse(request_id, bytes);
}
