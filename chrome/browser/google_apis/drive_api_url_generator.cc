// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_api_url_generator.h"

#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "chrome/common/net/url_util.h"

namespace google_apis {

namespace {

// Hard coded URLs for communication with a google drive server.
const char kDriveV2AboutUrl[] = "https://www.googleapis.com/drive/v2/about";
const char kDriveV2ApplistUrl[] = "https://www.googleapis.com/drive/v2/apps";
const char kDriveV2ChangelistUrl[] =
    "https://www.googleapis.com/drive/v2/changes";

const char kDriveV2FilelistUrl[] = "https://www.googleapis.com/drive/v2/files";
const char kDriveV2FileUrlFormat[] =
    "https://www.googleapis.com/drive/v2/files/%s";

}  // namespace

DriveApiUrlGenerator::DriveApiUrlGenerator() {
  // Do nothing.
}

DriveApiUrlGenerator::~DriveApiUrlGenerator() {
  // Do nothing.
}

GURL DriveApiUrlGenerator::GetAboutUrl() const {
  return GURL(kDriveV2AboutUrl);
}

GURL DriveApiUrlGenerator::GetApplistUrl() const {
  return GURL(kDriveV2ApplistUrl);
}

GURL DriveApiUrlGenerator::GetChangelistUrl(
    const GURL& override_url, int64 start_changestamp) const {
  // Use override_url if not empty,
  // otherwise use the default url (kDriveV2Changelisturl).
  const GURL& url =
      override_url.is_empty() ? GURL(kDriveV2ChangelistUrl) : override_url;
  return start_changestamp ?
      chrome_common_net::AppendOrReplaceQueryParameter(
          url, "startChangeId", base::Int64ToString(start_changestamp)) :
      url;
}

GURL DriveApiUrlGenerator::GetFilelistUrl(
    const GURL& override_url, const std::string& search_string) const {
  // Use override_url if not empty,
  // otherwise use the default url (kDriveV2FilelistUrl).
  const GURL& url =
      override_url.is_empty() ? GURL(kDriveV2FilelistUrl) : override_url;

  return search_string.empty() ?
      url :
      chrome_common_net::AppendOrReplaceQueryParameter(
          url, "q", search_string);
}

GURL DriveApiUrlGenerator::GetFileUrl(const std::string& file_id) const {
  return GURL(base::StringPrintf(kDriveV2FileUrlFormat, file_id.c_str()));
}

}  // namespace google_apis
