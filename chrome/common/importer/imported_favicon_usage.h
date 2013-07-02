// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTER_IMPORTED_FAVICON_USAGE_H_
#define CHROME_COMMON_IMPORTER_IMPORTED_FAVICON_USAGE_H_

#include <set>
#include <vector>

#include "url/gurl.h"

// Used to correlate favicons to imported bookmarks.
struct ImportedFaviconUsage {
  ImportedFaviconUsage();
  ~ImportedFaviconUsage();

  // The URL of the favicon.
  GURL favicon_url;

  // The raw png-encoded data.
  std::vector<unsigned char> png_data;

  // The list of URLs using this favicon.
  std::set<GURL> urls;
};

#endif  // CHROME_COMMON_IMPORTER_IMPORTED_FAVICON_USAGE_H_
