// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_source_helper.h"

#include <stdio.h>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "url/gurl.h"

namespace media_router {

namespace {

// Prefixes used to format and detect various protocols' media source URNs.
// See: https://www.ietf.org/rfc/rfc3406.txt
constexpr char kTabMediaUrnFormat[] = "urn:x-org.chromium.media:source:tab:%d";
constexpr char kDesktopMediaUrn[] = "urn:x-org.chromium.media:source:desktop";
constexpr char kCastUrnPrefix[] = "urn:x-com.google.cast:application:";
constexpr char kTabRemotingUrnFormat[] =
    "urn:x-org.chromium.media:source:tab_content_remoting:%d";

}  // namespace

MediaSource MediaSourceForTab(int tab_id) {
  return MediaSource(base::StringPrintf(kTabMediaUrnFormat, tab_id));
}

MediaSource MediaSourceForTabContentRemoting(content::WebContents* contents) {
  DCHECK(contents);
  return MediaSource(base::StringPrintf(kTabRemotingUrnFormat,
                                        SessionTabHelper::IdForTab(contents)));
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
  int tab_id;
  return sscanf(source.id().c_str(), kTabMediaUrnFormat, &tab_id) == 1 &&
      tab_id > 0;
}

bool IsMirroringMediaSource(const MediaSource& source) {
  return IsDesktopMirroringMediaSource(source) ||
         IsTabMirroringMediaSource(source);
}

int TabIdFromMediaSource(const MediaSource& source) {
  int tab_id;
  if (sscanf(source.id().c_str(), kTabMediaUrnFormat, &tab_id) == 1)
    return tab_id;
  else if (sscanf(source.id().c_str(), kTabRemotingUrnFormat, &tab_id) == 1)
    return tab_id;
  else
    return -1;
}

bool IsValidMediaSource(const MediaSource& source) {
  return (TabIdFromMediaSource(source) > 0 ||
          IsDesktopMirroringMediaSource(source) ||
          base::StartsWith(source.id(), kCastUrnPrefix,
                           base::CompareCase::SENSITIVE) ||
          IsValidPresentationUrl(source.id()));
}

std::string PresentationUrlFromMediaSource(const MediaSource& source) {
  return IsValidPresentationUrl(source.id()) ? source.id() : "";
}

bool IsValidPresentationUrl(const std::string& url) {
  GURL gurl(url);
  return gurl.is_valid() && gurl.SchemeIsHTTPOrHTTPS();
}

}  // namespace media_router
