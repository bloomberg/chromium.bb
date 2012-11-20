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

#ifndef NDEBUG
// Use smaller 'page' size while debugging to ensure we hit feed reload
// almost always. Be careful not to use something too small on account that
// have many items because server side 503 error might kick in.
const int kMaxDocumentsPerFeed = 500;
const int kMaxDocumentsPerSearchFeed = 50;
#else
const int kMaxDocumentsPerFeed = 500;
const int kMaxDocumentsPerSearchFeed = 50;
#endif

// URL requesting documents list that shared to the authenticated user only
const char kGetDocumentListURLForSharedWithMe[] =
    "https://docs.google.com/feeds/default/private/full/-/shared-with-me";

// URL requesting documents list of changes to documents collections.
const char kGetChangesListURL[] =
    "https://docs.google.com/feeds/default/private/changes";

// Generates a URL for getting document list. If |directory_resource_id| is
// empty, returns a URL for fetching all documents. If it's given, returns a
// URL for fetching documents in a particular directory.
// This function is used to implement GenerateGetDocumentsURL().
GURL GenerateDocumentListURL(const std::string& directory_resource_id) {
  if (directory_resource_id.empty())
    return GURL(kGetDocumentListURLForAllDocuments);

  return GURL(base::StringPrintf(kGetDocumentListURLForDirectoryFormat,
                                 net::EscapePath(
                                     directory_resource_id).c_str()));
}

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

GURL GenerateGetDocumentsURL(
    const GURL& override_url,
    int start_changestamp,
    const std::string& search_string,
    bool shared_with_me,
    const std::string& directory_resource_id) {
  int max_docs = search_string.empty() ? kMaxDocumentsPerFeed :
                                         kMaxDocumentsPerSearchFeed;

  if (!override_url.is_empty())
    return gdata_wapi_url_util::AddFeedUrlParams(override_url,
                                                 max_docs,
                                                 0,
                                                 search_string);

  if (shared_with_me) {
    return gdata_wapi_url_util::AddFeedUrlParams(
        GURL(kGetDocumentListURLForSharedWithMe),
        max_docs,
        0,
        search_string);
  }

  if (start_changestamp == 0) {
    return gdata_wapi_url_util::AddFeedUrlParams(
        gdata_wapi_url_util::GenerateDocumentListURL(directory_resource_id),
        max_docs,
        0,
        search_string);
  }

  return gdata_wapi_url_util::AddFeedUrlParams(
      GURL(kGetChangesListURL),
      kMaxDocumentsPerFeed,
      start_changestamp,
      std::string());
}

}  // namespace gdata_wapi_url_util
}  // namespace google_apis
