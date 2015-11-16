// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_source_helper.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/media_source.h"
#include "url/gurl.h"

namespace media_router {

// Prefixes used to format and detect various protocols' media source URNs.
// See: https://www.ietf.org/rfc/rfc3406.txt
const char kTabMediaUrnPrefix[] = "urn:x-org.chromium.media:source:tab";
const char kDesktopMediaUrn[] = "urn:x-org.chromium.media:source:desktop";
const char kCastUrnPrefix[] = "urn:x-com.google.cast:application:";

MediaSource MediaSourceForTab(int tab_id) {
  return MediaSource(base::StringPrintf("%s:%d", kTabMediaUrnPrefix, tab_id));
}

MediaSource MediaSourceForDesktop() {
  return MediaSource(std::string(kDesktopMediaUrn));
}

MediaSource MediaSourceForCastApp(const std::string& app_id) {
  return MediaSource(kCastUrnPrefix + app_id);
}

MediaSource MediaSourceForPresentationUrl(const std::string& presentation_url) {
  return MediaSource(presentation_url);
}

bool IsDesktopMirroringMediaSource(const MediaSource& source) {
  return base::StartsWith(source.id(), kDesktopMediaUrn,
                          base::CompareCase::SENSITIVE);
}

bool IsTabMirroringMediaSource(const MediaSource& source) {
  return base::StartsWith(source.id(), kTabMediaUrnPrefix,
                          base::CompareCase::SENSITIVE);
}

bool IsMirroringMediaSource(const MediaSource& source) {
  return IsDesktopMirroringMediaSource(source) ||
         IsTabMirroringMediaSource(source);
}

bool IsValidMediaSource(const MediaSource& source) {
  if (IsMirroringMediaSource(source) ||
      base::StartsWith(source.id(), kCastUrnPrefix,
                       base::CompareCase::SENSITIVE)) {
    return true;
  }
  GURL url(source.id());
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS();
}

std::string PresentationUrlFromMediaSource(const MediaSource& source) {
  return IsValidPresentationUrl(source.id()) ? source.id() : "";
}

bool IsValidPresentationUrl(const std::string& url) {
  GURL gurl(url);
  return gurl.is_valid() && gurl.SchemeIsHTTPOrHTTPS();
}

}  // namespace media_router
