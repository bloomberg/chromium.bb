// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_ICON_CACHER_H_
#define COMPONENTS_NTP_TILES_ICON_CACHER_H_

#include "base/callback.h"
#include "components/ntp_tiles/popular_sites.h"

namespace ntp_tiles {

// Ensures that a Popular Sites icon is cached, downloading and saving it if
// not.
//
// Does not provide any way to get a fetched favicon; use the FaviconService for
// that. All this interface guarantees is that FaviconService will be able to
// get you an icon (if it exists).
class IconCacher {
 public:
  virtual ~IconCacher() = default;

  // Fetches the icon if necessary, then invokes |done| with true if it was
  // newly fetched (false if it was already cached or could not be fetched).
  virtual void StartFetch(PopularSites::Site site,
                          const base::Callback<void(bool)>& done) = 0;
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_ICON_CACHER_H_
