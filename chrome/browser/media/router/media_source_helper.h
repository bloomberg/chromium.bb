// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SOURCE_HELPER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SOURCE_HELPER_H_

#include <string>

#include "chrome/browser/media/router/media_source.h"

namespace media_router {

// Helper library for protocol-specific media source object creation.
// Returns MediaSource URI depending on the type of source.
MediaSource MediaSourceForTab(int tab_id);
MediaSource MediaSourceForDesktop();
MediaSource MediaSourceForCastApp(const std::string& app_id);
MediaSource MediaSourceForPresentationUrl(const std::string& presentation_url);

// Returns true if |source| outputs its content via mirroring.
bool IsDesktopMirroringMediaSource(const MediaSource& source);
bool IsTabMirroringMediaSource(const MediaSource& source);
bool IsMirroringMediaSource(const MediaSource& source);

// Checks that |source| is a parseable URN and is of a known type.
// Does not deeper protocol-level syntax checks.
bool IsValidMediaSource(const MediaSource& source);

// Extracts the presentation URL from |source|.
// If |source| is invalid, an empty string is returned.
std::string PresentationUrlFromMediaSource(const MediaSource& source);

// Returns true if |source| is a valid presentation URL.
bool IsValidPresentationUrl(const std::string& url);

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SOURCE_HELPER_H_
