// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// URL utility functions for Google Documents List API (aka WAPI).

#ifndef CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_URL_GENERATOR_H_
#define CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_URL_GENERATOR_H_

#include <string>

#include "googleurl/src/gurl.h"

namespace google_apis {

// The class is used to generate URLs for communicating with the WAPI server.
// for production, and the local server for testing.
class GDataWapiUrlGenerator {
 public:
  explicit GDataWapiUrlGenerator(const GURL& base_url);
  ~GDataWapiUrlGenerator();

  // The base URL for communicating with the WAPI server for production.
  static const char kBaseUrlForProduction[];

  // Adds additional parameters for API version, output content type and to
  // show folders in the feed are added to document feed URLs.
  static GURL AddStandardUrlParams(const GURL& url);

  // Adds additional parameters to metadata feed to include installed 3rd
  // party applications.
  static GURL AddMetadataUrlParams(const GURL& url);

  // Adds additional parameters for API version, output content type and to
  // show folders in the feed are added to document feed URLs.
  // Optionally, adds start-index=... parameter if |changestamp| is non-zero,
  // and adds q=... parameter if |search_string| is non-empty.
  static GURL AddFeedUrlParams(const GURL& url,
                               int num_items_to_fetch,
                               int changestamp,
                               const std::string& search_string);

  // Generates a URL for getting the resource list feed.
  //
  // override_url:
  //   By default, a hard-coded base URL of the WAPI server is used.
  //   The base URL can be overridden by |override_url|.
  //   This is used for handling continuation of feeds (2nd page and onward).
  //
  // start_changestamp
  //   If |start_changestamp| is 0, URL for a full feed is generated.
  //   If |start_changestamp| is non-zero, URL for a delta feed is generated.
  //
  // search_string
  //   If |search_string| is non-empty, q=... parameter is added, and
  //   max-results=... parameter is adjusted for a search.
  //
  // shared_with_me
  //   If |shared_with_me| is true, the base URL is changed to fetch the
  //   shared-with-me documents.
  //
  // directory_resource_id:
  //   If |directory_resource_id| is non-empty, a URL for fetching documents in
  //   a particular directory is generated.
  //
  GURL GenerateResourceListUrl(
      const GURL& override_url,
      int start_changestamp,
      const std::string& search_string,
      bool shared_with_me,
      const std::string& directory_resource_id) const;

  // Generates a URL for getting the resource entry of the given resource ID.
  GURL GenerateResourceEntryUrl(const std::string& resource_id) const;

  // Generates a URL for getting the root resource list feed.
  // Used to make changes in the root directory (ex. create a directory in the
  // root directory)
  GURL GenerateResourceListRootUrl() const;

  // Generates a URL for getting the account metadata feed.
  GURL GenerateAccountMetadataUrl() const;

 private:
  const GURL base_url_;
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_URL_GENERATOR_H_
