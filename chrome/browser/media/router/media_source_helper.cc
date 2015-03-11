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

MediaSource ForTabMediaSource(int tab_id) {
  return MediaSource(base::StringPrintf("%s:%d", kTabMediaUrnPrefix, tab_id));
}

MediaSource ForDesktopMediaSource() {
  return MediaSource(std::string(kDesktopMediaUrn));
}

// TODO(mfoltz): Remove when the TODO in
// MediaSourceManager::GetDefaultMediaSource is resolved.
MediaSource ForCastAppMediaSource(const std::string& app_id) {
  return MediaSource(kCastUrnPrefix + app_id);
}

MediaSource ForPresentationUrl(const std::string& presentation_url) {
  return MediaSource(presentation_url);
}

bool IsMirroringMediaSource(const MediaSource& source) {
  return StartsWithASCII(source.id(), kDesktopMediaUrn, true) ||
    StartsWithASCII(source.id(), kTabMediaUrnPrefix, true);
}

bool IsValidMediaSource(const MediaSource& source) {
  if (IsMirroringMediaSource(source) ||
      StartsWithASCII(source.id(), kCastUrnPrefix, true)) {
    return true;
  }
  GURL url(source.id());
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS();
}

}  // namespace media_router
