// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_INSTALLABLE_DATA_H_
#define CHROME_BROWSER_INSTALLABLE_INSTALLABLE_DATA_H_

#include "chrome/browser/installable/installable_logging.h"
#include "content/public/common/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

// This struct is passed to an InstallableCallback when the InstallableManager
// has finished working. Each reference is owned by InstallableManager, and
// callers should copy any objects which they wish to use later. Non-requested
// fields will be set to null, empty, or false.
struct InstallableData {
  // NO_ERROR_DETECTED if there were no issues.
  const InstallableStatusCode error_code;

  // Empty if the site has no <link rel="manifest"> tag.
  const GURL& manifest_url;

  // Empty if the site has an unparseable manifest.
  const content::Manifest& manifest;

  // Empty if no primary_icon was requested.
  const GURL& primary_icon_url;

  // nullptr if the most appropriate primary icon couldn't be determined or
  // downloaded. The underlying primary icon is owned by the InstallableManager;
  // clients must copy the bitmap if they want to to use it. If
  // fetch_valid_primary_icon was true and a primary icon could not be
  // retrieved, the reason will be in error_code.
  const SkBitmap* primary_icon;

  // Empty if no badge_icon was requested.
  const GURL& badge_icon_url;

  // nullptr if the most appropriate badge icon couldn't be determined or
  // downloaded. The underlying badge icon is owned by the InstallableManager;
  // clients must copy the bitmap if they want to to use it. Since the badge
  // icon is optional, no error code is set if it cannot be fetched, and clients
  // specifying fetch_valid_badge_icon must check that the bitmap exists before
  // using it.
  const SkBitmap* badge_icon;

  // true if the site has a service worker with a fetch handler and a viable web
  // app manifest. If check_installable was true and the site isn't installable,
  // the reason will be in error_code.
  const bool is_installable;
};

using InstallableCallback = base::Callback<void(const InstallableData&)>;

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLABLE_DATA_H_
