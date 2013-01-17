// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "chrome/common/net/url_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"

namespace google_apis {
namespace {

// URL requesting resource list that belong to the authenticated user only
// (handled with '/-/mine' part).
const char kGetResourceListURLForAllDocuments[] =
    "/feeds/default/private/full/-/mine";

// URL requesting resource list in a particular directory specified by "%s"
// that belong to the authenticated user only (handled with '/-/mine' part).
const char kGetResourceListURLForDirectoryFormat[] =
    "/feeds/default/private/full/%s/contents/-/mine";

// URL requesting single resource entry whose resource id is specified by "%s".
const char kGetResourceEntryURLFormat[] = "/feeds/default/private/full/%s";

// Root resource list url.
const char kResourceListRootURL[] = "/feeds/default/private/full";

// Metadata feed with things like user quota.
const char kAccountMetadataURL[] = "/feeds/metadata/default";

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
const char kGetResourceListURLForSharedWithMe[] =
    "/feeds/default/private/full/-/shared-with-me";

// URL requesting documents list of changes to documents collections.
const char kGetChangesListURL[] = "/feeds/default/private/changes";

}  // namespace

const char GDataWapiUrlGenerator::kBaseUrlForProduction[] =
    "https://docs.google.com/";

// static
GURL GDataWapiUrlGenerator::AddStandardUrlParams(const GURL& url) {
  GURL result =
      chrome_common_net::AppendOrReplaceQueryParameter(url, "v", "3");
  result =
      chrome_common_net::AppendOrReplaceQueryParameter(result, "alt", "json");
  return result;
}

// static
GURL GDataWapiUrlGenerator::AddMetadataUrlParams(const GURL& url) {
  GURL result = AddStandardUrlParams(url);
  result = chrome_common_net::AppendOrReplaceQueryParameter(
      result, "include-installed-apps", "true");
  return result;
}

// static
GURL GDataWapiUrlGenerator::AddFeedUrlParams(
    const GURL& url,
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

GDataWapiUrlGenerator::GDataWapiUrlGenerator(const GURL& base_url)
    : base_url_(GURL(base_url)) {
}

GDataWapiUrlGenerator::~GDataWapiUrlGenerator() {
}

GURL GDataWapiUrlGenerator::GenerateResourceListUrl(
    const GURL& override_url,
    int start_changestamp,
    const std::string& search_string,
    bool shared_with_me,
    const std::string& directory_resource_id) const {
  DCHECK_LE(0, start_changestamp);

  int max_docs = search_string.empty() ? kMaxDocumentsPerFeed :
                                         kMaxDocumentsPerSearchFeed;
  GURL url;
  if (!override_url.is_empty()) {
    // |override_url| specifies the URL of the continuation feed when the feed
    // is broken up to multiple chunks. In this case we must not add the
    // |start_changestamp| that provides the original start point.
    start_changestamp = 0;
    url = override_url;
  } else if (shared_with_me) {
    url = base_url_.Resolve(kGetResourceListURLForSharedWithMe);
  } else if (start_changestamp > 0) {
    // The start changestamp shouldn't be used for a search.
    DCHECK(search_string.empty());
    url = base_url_.Resolve(kGetChangesListURL);
  } else if (!directory_resource_id.empty()) {
    url = base_url_.Resolve(
        base::StringPrintf(kGetResourceListURLForDirectoryFormat,
                           net::EscapePath(
                               directory_resource_id).c_str()));
  } else {
    url = base_url_.Resolve(kGetResourceListURLForAllDocuments);
  }
  return AddFeedUrlParams(url, max_docs, start_changestamp, search_string);
}

GURL GDataWapiUrlGenerator::GenerateResourceEntryUrl(
    const std::string& resource_id) const {
  GURL result = base_url_.Resolve(
      base::StringPrintf(kGetResourceEntryURLFormat,
                         net::EscapePath(resource_id).c_str()));
  return AddStandardUrlParams(result);
}

GURL GDataWapiUrlGenerator::GenerateResourceListRootUrl() const {
  return AddStandardUrlParams(base_url_.Resolve(kResourceListRootURL));
}

GURL GDataWapiUrlGenerator::GenerateAccountMetadataUrl() const {
  return AddMetadataUrlParams(base_url_.Resolve(kAccountMetadataURL));
}

}  // namespace google_apis
