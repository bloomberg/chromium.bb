// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// URL utility functions for Google Documents List API (aka WAPI).

#ifndef CHROME_BROWSER_GOOGLE_APIS_URL_UTIL_H_
#define CHROME_BROWSER_GOOGLE_APIS_URL_UTIL_H_

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

// Formats a URL for getting document list. If |directory_resource_id| is
// empty, returns a URL for fetching all documents. If it's given, returns a
// URL for fetching documents in a particular directory.
GURL FormatDocumentListURL(const std::string& directory_resource_id);

}  // namespace gdata_wapi_url_util
}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_URL_UTIL_H_
