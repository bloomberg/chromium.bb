// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/gdata_wapi_url_util.h"

#include "base/stringprintf.h"
#include "chrome/common/net/url_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"

namespace google_apis {
namespace gdata_wapi_url_util {

namespace {

// URL requesting documents list that belong to the authenticated user only
// (handled with '/-/mine' part).
const char kGetDocumentListURLForAllDocuments[] =
    "https://docs.google.com/feeds/default/private/full/-/mine";

// URL requesting documents list in a particular directory specified by "%s"
// that belong to the authenticated user only (handled with '/-/mine' part).
const char kGetDocumentListURLForDirectoryFormat[] =
    "https://docs.google.com/feeds/default/private/full/%s/contents/-/mine";

}  // namespace

GURL AddStandardUrlParams(const GURL& url) {
  GURL result =
      chrome_common_net::AppendOrReplaceQueryParameter(url, "v", "3");
  result =
      chrome_common_net::AppendOrReplaceQueryParameter(result, "alt", "json");
  return result;
}

GURL AddMetadataUrlParams(const GURL& url) {
  GURL result = AddStandardUrlParams(url);
  result = chrome_common_net::AppendOrReplaceQueryParameter(
      result, "include-installed-apps", "true");
  return result;
}

GURL AddFeedUrlParams(const GURL& url,
                      int num_items_to_fetch,
                      int changestamp,
                      const std::string& search_string) {
  GURL result = AddStandardUrlParams(url);
  result = chrome_common_net::AppendOrReplaceQueryParameter(
      result,
      "showfolders",
      "true");
  result = chrome_common_net::AppendOrReplaceQueryParameter(
      result,
      "max-results",
      base::StringPrintf("%d", num_items_to_fetch));
  result = chrome_common_net::AppendOrReplaceQueryParameter(
      result, "include-installed-apps", "true");

  if (changestamp) {
    result = chrome_common_net::AppendQueryParameter(
        result,
        "start-index",
        base::StringPrintf("%d", changestamp));
  }

  if (!search_string.empty()) {
    result = chrome_common_net::AppendOrReplaceQueryParameter(
        result, "q", search_string);
  }
  return result;
}

GURL FormatDocumentListURL(const std::string& directory_resource_id) {
  if (directory_resource_id.empty())
    return GURL(kGetDocumentListURLForAllDocuments);

  return GURL(base::StringPrintf(kGetDocumentListURLForDirectoryFormat,
                                 net::EscapePath(
                                     directory_resource_id).c_str()));
}

}  // namespace gdata_wapi_url_util
}  // namespace google_apis
