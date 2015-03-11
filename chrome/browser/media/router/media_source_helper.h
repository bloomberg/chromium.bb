// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SOURCE_HELPER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SOURCE_HELPER_H_

#include <string>

#include "chrome/browser/media/router/media_source.h"

namespace media_router {

// Helper library for protocol-specific media source object creation.

// TODO(kmarshall): Should these creation methods be moved to the WebUI, which
// (excluding Presentation API) is their only caller? The MR base code can be
// Also, consider generating an is_mirroring state boolean at the time of URN
// creation so that the mirroring status does not have to be determined from a
// string prefix check.
// These changes would allow the MR to handle MediaSource objects in the same
// type agnostic fashion vs. having to format and parse URNs and track which
// MediaSource types are mirroring-enabled.

// Returns MediaSource URI depending on the type of source.
MediaSource ForTabMediaSource(int tab_id);
MediaSource ForDesktopMediaSource();
MediaSource ForCastAppMediaSource(const std::string& app_id);
MediaSource ForPresentationUrl(const std::string& presentation_url);

// Returns true if |source| outputs its content via mirroring.
bool IsMirroringMediaSource(const MediaSource& source);

// Checks that |source| is a parseable URN and is of a known type.
// Does not deeper protocol-level syntax checks.
bool IsValidMediaSource(const MediaSource& source);

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SOURCE_HELPER_H_
