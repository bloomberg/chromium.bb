// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_server_urls.h"

#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"

namespace offline_pages {

namespace {

const char kPrefetchServer[] = "https://offlinepages-pa.googleapis.com/";
const char kPrefetchStagingServer[] =
    "https://staging-offlinepages-pa.sandbox.googleapis.com/";

const char kGeneratePageBundleRequestURLPath[] = "v1:GeneratePageBundle";
const char kGetOperationLeadingURLPath[] = "v1/";
const char kDownloadLeadingURLPath[] = "v1/media/";

// Used in all offline prefetch request URLs to specify API Key.
const char kApiKeyName[] = "key";
// Needed to download as a file.
const char kAltKeyName[] = "alt";
const char kAltKeyValueForDownload[] = "media";

GURL GetServerURLForPath(const std::string& url_path,
                         version_info::Channel channel) {
  bool is_stable_channel = channel == version_info::Channel::STABLE;
  GURL server_url(is_stable_channel ? kPrefetchServer : kPrefetchStagingServer);

  GURL::Replacements replacements;
  replacements.SetPathStr(url_path);
  return server_url.ReplaceComponents(replacements);
}

GURL AppendApiKeyToURL(const GURL& url, version_info::Channel channel) {
  bool is_stable_channel = channel == version_info::Channel::STABLE;
  std::string api_key = is_stable_channel ? google_apis::GetAPIKey()
                                          : google_apis::GetNonStableAPIKey();
  return net::AppendQueryParameter(url, kApiKeyName, api_key);
}

}  // namespace

GURL GeneratePageBundleRequestURL(version_info::Channel channel) {
  GURL server_url =
      GetServerURLForPath(kGeneratePageBundleRequestURLPath, channel);
  return AppendApiKeyToURL(server_url, channel);
}

GURL GetOperationRequestURL(const std::string& name,
                            version_info::Channel channel) {
  std::string url_path = kGetOperationLeadingURLPath + name;
  GURL server_url = GetServerURLForPath(url_path, channel);
  return AppendApiKeyToURL(server_url, channel);
}

GURL PrefetchDownloadURL(const std::string& download_location,
                         version_info::Channel channel) {
  std::string url_path = kDownloadLeadingURLPath + download_location;
  GURL server_url = GetServerURLForPath(url_path, channel);

  server_url = net::AppendQueryParameter(server_url, kAltKeyName,
                                         kAltKeyValueForDownload);

  return AppendApiKeyToURL(server_url, channel);
}

}  // namespace offline_pages
