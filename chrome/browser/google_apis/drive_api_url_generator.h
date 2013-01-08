// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_URL_GENERATOR_H_
#define CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_URL_GENERATOR_H_

#include <string>

#include "googleurl/src/gurl.h"

namespace google_apis {

// This class is used to generate URLs for communicating with drive api
// servers for production, and a local server for testing.
class DriveApiUrlGenerator {
 public:
  // TODO(hidehiko): Pass server name to a constructor in order to inject
  // server path for testing.
  DriveApiUrlGenerator();
  ~DriveApiUrlGenerator();

  // Returns a URL to fetch "about" data.
  GURL GetAboutUrl() const;

  // Returns a URL to fetch "applist" data.
  GURL GetApplistUrl() const;

  // Returns a URL to fetch a list of changes.
  // override_url:
  //   The base url for the fetch. If empty, the default url is used.
  // start_changestamp:
  //   The starting point of the requesting change list, or 0 if all changes
  //   are necessary.
  GURL GetChangelistUrl(
      const GURL& override_url, int64 start_changestamp) const;

  // Returns a URL to fetch a list of files with the given |search_string|.
  // override_url:
  //   The base url for the fetching. If empty, the default url is used.
  // search_string: The search query.
  GURL GetFilelistUrl(
      const GURL& override_url, const std::string& search_string) const;

  // Returns a URL to fecth a file content.
  GURL GetFileUrl(const std::string& file_id) const;

  // This class is copyable hence no DISALLOW_COPY_AND_ASSIGN here.
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_URL_GENERATOR_H_
