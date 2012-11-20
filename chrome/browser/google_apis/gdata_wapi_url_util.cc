// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/gdata_wapi_url_util.h"

#include "base/logging.h"
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

// URL requesting single document entry whose resource id is specified by "%s".
const char kGetDocumentEntryURLFormat[] =
    "https://docs.google.com/feeds/default/private/full/%s";

// Root document list url.
const char kDocumentListRootURL[] =
    "https://docs.google.com/feeds/default/private/full";

// Metadata feed with things like user quota.
const char kAccountMetadataURL[] =
    "https://docs.google.com/feeds/metadata/default";

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

GURL GenerateDocumentListUrl(
    const GURL& override_url,
    int start_changestamp,
    const std::string& search_string,
    bool shared_with_me,
    const std::string& directory_resource_id) {
  DCHECK_LE(0, start_changestamp);

  int max_docs = search_string.empty() ? kMaxDocumentsPerFeed :
                                         kMaxDocumentsPerSearchFeed;
  GURL url;
  if (!override_url.is_empty()) {
    url = override_url;
  } else if (shared_with_me) {
    url = GURL(kGetDocumentListURLForSharedWithMe);
  } else if (start_changestamp > 0) {
    // The start changestamp shouldn't be used for a search.
    DCHECK(search_string.empty());
    url = GURL(kGetChangesListURL);
  } else if (!directory_resource_id.empty()) {
    url = GURL(base::StringPrintf(kGetDocumentListURLForDirectoryFormat,
                                  net::EscapePath(
                                      directory_resource_id).c_str()));
  } else {
    url = GURL(kGetDocumentListURLForAllDocuments);
  }
  return AddFeedUrlParams(url, max_docs, start_changestamp, search_string);
}

GURL GenerateDocumentEntryUrl(const std::string& resource_id) {
  GURL result = GURL(base::StringPrintf(kGetDocumentEntryURLFormat,
                                        net::EscapePath(resource_id).c_str()));
  return AddStandardUrlParams(result);
}

GURL GenerateDocumentListRootUrl() {
  return AddStandardUrlParams(GURL(kDocumentListRootURL));
}

GURL GenerateAccountMetadataUrl() {
  return AddMetadataUrlParams(GURL(kAccountMetadataURL));
}

}  // namespace gdata_wapi_url_util
}  // namespace google_apis
