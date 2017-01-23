// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_POPULAR_SITES_H_
#define COMPONENTS_NTP_TILES_POPULAR_SITES_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "url/gurl.h"

namespace base {
class ListValue;
}

namespace ntp_tiles {

// Interface to provide a list of suggested popular sites, for display on the
// NTP when there are not enough personalized tiles.
class PopularSites {
 public:
  struct Site {
    Site(const base::string16& title,
         const GURL& url,
         const GURL& favicon_url,
         const GURL& large_icon_url,
         const GURL& thumbnail_url);
    Site(const Site& other);
    ~Site();

    base::string16 title;
    GURL url;
    GURL favicon_url;
    GURL large_icon_url;
    GURL thumbnail_url;
  };

  using SitesVector = std::vector<Site>;
  using FinishedCallback = base::Callback<void(bool /* success */)>;

  virtual ~PopularSites() = default;

  // Starts the process of retrieving popular sites. When they are available,
  // invokes |callback| with the result, on the same thread as the caller. Never
  // invokes |callback| before returning control to the caller, even if the
  // result is immediately known.
  //
  // Set |force_download| to enforce re-downloading the popular sites JSON, even
  // if it already exists in cache.
  //
  // Must be called at most once on a given PopularSites object.
  // TODO(mastiz): Remove this restriction?
  virtual void StartFetch(bool force_download,
                          const FinishedCallback& callback) = 0;

  // Returns the list of available sites.
  virtual const SitesVector& sites() const = 0;

  // Various internals exposed publicly for diagnostic pages only.
  virtual GURL GetLastURLFetched() const = 0;
  virtual GURL GetURLToFetch() = 0;
  virtual std::string GetCountryToFetch() = 0;
  virtual std::string GetVersionToFetch() = 0;
  virtual const base::ListValue* GetCachedJson() = 0;
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_POPULAR_SITES_H_
