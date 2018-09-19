// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_TYPES_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_TYPES_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace explore_sites {
// The in-memory representation of a site in the ExploreSitesStore.
// Image data is not represented here because it is requested separately from
// the UI layer.
struct ExploreSitesSite {
  ExploreSitesSite(int site_id, int category_id, GURL url, std::string title);
  ExploreSitesSite(ExploreSitesSite&& other);
  virtual ~ExploreSitesSite();

  int site_id;
  int category_id;
  GURL url;
  std::string title;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesSite);
};

// The in-memory representation of a category in the ExploreSitesStore.
// Image data is not represented here because it is requested separately from
// the UI layer.
struct ExploreSitesCategory {
  // Creates a category.  Sites should be populated separately.
  ExploreSitesCategory(int category_id,
                       int version,
                       int category_type,
                       std::string label);
  ExploreSitesCategory(ExploreSitesCategory&& other);
  virtual ~ExploreSitesCategory();

  int category_id;
  int version;
  int category_type;
  std::string label;

  std::vector<ExploreSitesSite> sites;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesCategory);
};

using CatalogCallback = base::OnceCallback<void(
    std::unique_ptr<std::vector<ExploreSitesCategory>>)>;
using EncodedImageBytes = std::vector<uint8_t>;
using EncodedImageList = std::vector<std::unique_ptr<EncodedImageBytes>>;
using EncodedImageListCallback = base::OnceCallback<void(EncodedImageList)>;

using BitmapCallback = base::OnceCallback<void(std::unique_ptr<SkBitmap>)>;
}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_TYPES_H_
