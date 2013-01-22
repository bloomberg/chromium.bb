// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/favicon_source.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/url_constants.h"
#include "grit/locale_settings.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Parameters which can be used in chrome://favicon path. See .h file for a
// description of what each does.
const char kSizeParameter[] = "size/";
const char kIconURLParameter[] = "iconurl/";
const char kOriginParameter[] = "origin/";

// Returns true if |search| is a substring of |path| which starts at
// |start_index|.
bool HasSubstringAt(const std::string& path,
                    size_t start_index,
                    const std::string& search) {
  if (search.empty())
    return false;

  if (start_index + search.size() >= path.size())
    return false;

  return (path.compare(start_index, search.size(), search) == 0);
}

}  // namespace

FaviconSource::IconRequest::IconRequest()
    : size_in_dip(gfx::kFaviconSize),
      scale_factor(ui::SCALE_FACTOR_NONE) {
}

FaviconSource::IconRequest::IconRequest(
    const content::URLDataSource::GotDataCallback& cb,
    const std::string& path,
    int size,
    ui::ScaleFactor scale)
    : callback(cb),
      request_path(path),
      size_in_dip(size),
      scale_factor(scale) {
}

FaviconSource::IconRequest::~IconRequest() {
}

FaviconSource::FaviconSource(Profile* profile, IconType type)
    : profile_(profile->GetOriginalProfile()),
      icon_types_(type == FAVICON ? history::FAVICON :
          history::TOUCH_PRECOMPOSED_ICON | history::TOUCH_ICON |
          history::FAVICON) {
}

FaviconSource::~FaviconSource() {
}

std::string FaviconSource::GetSource() {
  return icon_types_ == history::FAVICON ?
      chrome::kChromeUIFaviconHost : chrome::kChromeUITouchIconHost;
}

void FaviconSource::StartDataRequest(
    const std::string& path,
    bool is_incognito,
    const content::URLDataSource::GotDataCallback& callback) {
  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (!favicon_service || path.empty()) {
    SendDefaultResponse(callback);
    return;
  }

  DCHECK_EQ(16, gfx::kFaviconSize);
  int size_in_dip = 16;
  ui::ScaleFactor scale_factor = ui::SCALE_FACTOR_100P;

  size_t parsed_index = 0;
  if (HasSubstringAt(path, parsed_index, kSizeParameter)) {
    parsed_index += strlen(kSizeParameter);

    size_t scale_delimiter = path.find("@", parsed_index);
    if (scale_delimiter == std::string::npos) {
      SendDefaultResponse(callback);
      return;
    }

    std::string size = path.substr(parsed_index,
                                   scale_delimiter - parsed_index);
    if (!base::StringToInt(size, &size_in_dip)) {
      SendDefaultResponse(callback);
      return;
    }

    if (size_in_dip != 64 && size_in_dip != 32) {
      // Only 64x64, 32x32 and 16x16 icons are supported.
      size_in_dip = 16;
    }

    size_t slash = path.find("/", scale_delimiter);
    if (slash == std::string::npos) {
      SendDefaultResponse(callback);
      return;
    }

    std::string scale_str = path.substr(scale_delimiter + 1,
                                        slash - scale_delimiter - 1);
    web_ui_util::ParseScaleFactor(scale_str, &scale_factor);

    if (scale_factor != ui::SCALE_FACTOR_100P && size_in_dip != 16) {
      SendDefaultResponse(callback);
      return;
    }

    parsed_index = slash + 1;
  }

  if (HasSubstringAt(path, parsed_index, kIconURLParameter)) {
    parsed_index += strlen(kIconURLParameter);
    // TODO(michaelbai): Change GetRawFavicon to support combination of
    // IconType.
    favicon_service->GetRawFavicon(
        GURL(path.substr(parsed_index)),
        history::FAVICON,
        size_in_dip,
        scale_factor,
        base::Bind(&FaviconSource::OnFaviconDataAvailable,
                   base::Unretained(this),
                   IconRequest(callback,
                               path.substr(parsed_index),
                               size_in_dip,
                               scale_factor)),
        &cancelable_task_tracker_);
  } else {
    std::string url;

    // URL requests prefixed with "origin/" are converted to a form with an
    // empty path and a valid scheme. (e.g., example.com -->
    // http://example.com/ or http://example.com/a --> http://example.com/)
    if (HasSubstringAt(path, parsed_index, kOriginParameter)) {
      parsed_index += strlen(kOriginParameter);
      url = path.substr(parsed_index);

      // If the URL does not specify a scheme (e.g., example.com instead of
      // http://example.com), add "http://" as a default.
      if (!GURL(url).has_scheme())
        url = "http://" + url;

      // Strip the path beyond the top-level domain.
      url = GURL(url).GetOrigin().spec();
    } else {
      url = path.substr(parsed_index);
    }

    // Intercept requests for prepopulated pages.
    for (size_t i = 0; i < arraysize(history::kPrepopulatedPages); i++) {
      if (url ==
          l10n_util::GetStringUTF8(history::kPrepopulatedPages[i].url_id)) {
        callback.Run(
            ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
                history::kPrepopulatedPages[i].favicon_id,
                scale_factor));
        return;
      }
    }

    favicon_service->GetRawFaviconForURL(
        FaviconService::FaviconForURLParams(
            profile_, GURL(url), icon_types_, size_in_dip),
        scale_factor,
        base::Bind(&FaviconSource::OnFaviconDataAvailable,
                   base::Unretained(this),
                   IconRequest(callback,
                               url,
                               size_in_dip,
                               scale_factor)),
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
  // No additional checks to locate the favicon resource in the base
  // implementation.
  return false;
}

void FaviconSource::OnFaviconDataAvailable(
    const IconRequest& request,
    const history::FaviconBitmapResult& bitmap_result) {
  if (bitmap_result.is_valid()) {
    // Forward the data along to the networking system.
    request.callback.Run(bitmap_result.bitmap_data);
  } else if (!HandleMissingResource(request)) {
    SendDefaultResponse(request);
  }
}

void FaviconSource::SendDefaultResponse(
    const content::URLDataSource::GotDataCallback& callback) {
  SendDefaultResponse(IconRequest(callback,
                                  "",
                                  16,
                                  ui::SCALE_FACTOR_100P));
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
  base::RefCountedMemory* default_favicon = default_favicons_[favicon_index];

  if (!default_favicon) {
    ui::ScaleFactor scale_factor = icon_request.scale_factor;
    default_favicon = ResourceBundle::GetSharedInstance()
        .LoadDataResourceBytesForScale(resource_id, scale_factor);
    default_favicons_[favicon_index] = default_favicon;
  }

  icon_request.callback.Run(default_favicon);
}
