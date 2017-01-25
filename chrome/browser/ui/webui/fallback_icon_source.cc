// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/fallback_icon_source.h"

#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/common/url_constants.h"
#include "components/favicon/core/fallback_icon_service.h"
#include "components/favicon_base/fallback_icon_url_parser.h"
#include "components/keyed_service/core/service_access_type.h"
#include "net/url_request/url_request.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

FallbackIconSource::FallbackIconSource(
    favicon::FallbackIconService* fallback_icon_service)
    : fallback_icon_service_(fallback_icon_service) {
}

FallbackIconSource::~FallbackIconSource() {
}

std::string FallbackIconSource::GetSource() const {
  return chrome::kChromeUIFallbackIconHost;
}

void FallbackIconSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  chrome::ParsedFallbackIconPath parsed;
  bool success = parsed.Parse(path);
  if (!success) {
    SendDefaultResponse(callback);
    return;
  }

  GURL url(parsed.url_string());
  if (url.is_empty() || !url.is_valid()) {
    SendDefaultResponse(callback);
    return;
  }

  SendFallbackIconHelper(
      url, parsed.size_in_pixels(), parsed.style(), callback);
}

std::string FallbackIconSource::GetMimeType(const std::string&) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

bool FallbackIconSource::AllowCaching() const {
  return false;
}

bool FallbackIconSource::ShouldReplaceExistingSource() const {
  // Leave the existing DataSource in place, otherwise we'll drop any pending
  // requests on the floor.
  return false;
}

bool FallbackIconSource::ShouldServiceRequest(
    const net::URLRequest* request) const {
  if (request->url().SchemeIs(chrome::kChromeSearchScheme))
    return InstantIOContext::ShouldServiceRequest(request);
  return URLDataSource::ShouldServiceRequest(request);
}

void FallbackIconSource::SendFallbackIconHelper(
    const GURL& url,
    int size_in_pixels,
    const favicon_base::FallbackIconStyle& style,
    const content::URLDataSource::GotDataCallback& callback) {
  if (!fallback_icon_service_) {  // Can be null for tests.
    callback.Run(nullptr);  // Trigger "Not Found" response.
    return;
  }
  std::vector<unsigned char> bitmap_data =
      fallback_icon_service_->RenderFallbackIconBitmap(
          url, size_in_pixels, style);
  callback.Run(base::RefCountedBytes::TakeVector(&bitmap_data));
}

void FallbackIconSource::SendDefaultResponse(
    const content::URLDataSource::GotDataCallback& callback) {
  favicon_base::FallbackIconStyle default_style;
  SendFallbackIconHelper(GURL(), gfx::kFaviconSize, default_style, callback);
}
