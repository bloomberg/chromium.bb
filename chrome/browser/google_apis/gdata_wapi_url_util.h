// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// URL utility functions for Google Documents List API (aka WAPI).

#ifndef CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_URL_UTIL_H_
#define CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_URL_UTIL_H_

#include <string>

class GURL;

namespace google_apis {
namespace gdata_wapi_url_util {

// Adds additional parameters for API version, output content type and to show
// folders in the feed are added to document feed URLs.
GURL AddStandardUrlParams(const GURL& url);

// Adds additional parameters to metadata feed to include installed 3rd party
// applications.
GURL AddMetadataUrlParams(const GURL& url);

// Adds additional parameters for API version, output content type and to show
// folders in the feed are added to document feed URLs.
// Optionally, adds start-index=... parameter if |changestamp| is non-zero,
// and adds q=... parameter if |search_string| is non-empty.
GURL AddFeedUrlParams(const GURL& url,
                      int num_items_to_fetch,
                      int changestamp,
                      const std::string& search_string);

// Generates a URL for getting the documents list feed.
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
GURL GenerateDocumentListUrl(
    const GURL& override_url,
    int start_changestamp,
    const std::string& search_string,
    bool shared_with_me,
    const std::string& directory_resource_id);

// Generates a URL for getting the document entry of the given resource ID.
GURL GenerateDocumentEntryUrl(const std::string& resource_id);

// Generates a URL for getting the root document list feed.
// Used to make changes in the root directory (ex. create a directory in the
// root directory)
GURL GenerateDocumentListRootUrl();

// Generates a URL for getting the account metadata feed.
GURL GenerateAccountMetadataUrl();

}  // namespace gdata_wapi_url_util
}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_URL_UTIL_H_
