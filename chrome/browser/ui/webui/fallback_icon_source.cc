// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/fallback_icon_source.h"

#include <string>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/common/favicon/fallback_icon_url_parser.h"
#include "chrome/common/url_constants.h"
#include "grit/platform_locale_settings.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/favicon_size.h"

FallbackIconSource::FallbackIconSource() {
  std::vector<std::string> font_list;
#if defined(OS_CHROMEOS)
  font_list.push_back("Noto Sans");
#elif defined(OS_IOS)
  font_list.push_back("Helvetica Neue");
#else
  font_list.push_back(l10n_util::GetStringUTF8(IDS_SANS_SERIF_FONT_FAMILY));
#endif
  fallback_icon_service_.reset(
      new favicon_base::FallbackIconService(font_list));
}

FallbackIconSource::~FallbackIconSource() {
}

std::string FallbackIconSource::GetSource() const {
  return chrome::kChromeUIFallbackIconHost;
}

void FallbackIconSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  chrome::ParsedFallbackIconPath parsed;
  bool success = parsed.Parse(path);
  if (!success) {
    SendDefaultResponse(callback);
    return;
  }

  GURL url(parsed.url());
  std::vector<unsigned char> bitmap_data =
      fallback_icon_service_->RenderFallbackIconBitmap(
          url, parsed.size_in_pixels(), parsed.style());
  callback.Run(base::RefCountedBytes::TakeVector(&bitmap_data));
}

std::string FallbackIconSource::GetMimeType(const std::string&) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
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

void FallbackIconSource::SendDefaultResponse(
    const content::URLDataSource::GotDataCallback& callback) {
  std::vector<unsigned char> bitmap_data =
      fallback_icon_service_->RenderFallbackIconBitmap(
          GURL(), gfx::kFaviconSize, favicon_base::FallbackIconStyle());
  callback.Run(base::RefCountedBytes::TakeVector(&bitmap_data));
}
