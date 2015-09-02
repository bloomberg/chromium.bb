// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_CAST_MODE_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_CAST_MODE_H_

#include <set>
#include <string>

namespace media_router {

// A cast mode represents one way that the current WebContents (i.e., tab) may
// be presented to a media sink.  These must be declared in the priority order
// returned by GetPreferredCastMode.
enum MediaCastMode {
  // The default presentation for the WebContents.  Only available when the
  // document has provided a default presentation URL.
  DEFAULT,
  // Capture the rendered WebContents and stream it to a media sink.  Always
  // available.
  TAB_MIRROR,
  // Capture the entire desktop and stream it to a media sink.  Always
  // available.
  DESKTOP_MIRROR,
  // The number of cast modes; not a valid cast mode.  Add new cast modes above.
  NUM_CAST_MODES,
};

using CastModeSet = std::set<MediaCastMode>;

// Returns a localized description string for |mode| and |host|
// (e.g. google.com).
std::string MediaCastModeToDescription(MediaCastMode mode,
                                       const std::string& host);

// Returns true if |cast_mode_num| is a valid MediaCastMode, false otherwise.
bool IsValidCastModeNum(int cast_mode_num);

// Returns the preferred cast mode from the current set of cast modes.
// There must be at least one cast mode in |cast_modes|.
MediaCastMode GetPreferredCastMode(const CastModeSet& cast_modes);

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_CAST_MODE_H_
