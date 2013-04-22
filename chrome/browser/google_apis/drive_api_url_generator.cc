// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_api_url_generator.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"

namespace google_apis {

namespace {

// Hard coded URLs for communication with a google drive server.
const char kDriveV2AboutUrl[] = "/drive/v2/about";
const char kDriveV2ApplistUrl[] = "/drive/v2/apps";
const char kDriveV2ChangelistUrl[] = "/drive/v2/changes";
const char kDriveV2FilesUrl[] = "/drive/v2/files";
const char kDriveV2FileUrlPrefix[] = "/drive/v2/files/";
const char kDriveV2ChildrenUrlFormat[] = "/drive/v2/files/%s/children";
const char kDriveV2ChildrenUrlForRemovalFormat[] =
    "/drive/v2/files/%s/children/%s";
const char kDriveV2FileCopyUrlFormat[] = "/drive/v2/files/%s/copy";
const char kDriveV2FileTrashUrlFormat[] = "/drive/v2/files/%s/trash";
const char kDriveV2InitiateUploadNewFileUrl[] = "/upload/drive/v2/files";
const char kDriveV2InitiateUploadExistingFileUrlPrefix[] =
    "/upload/drive/v2/files/";

GURL AddResumableUploadParam(const GURL& url) {
  return net::AppendOrReplaceQueryParameter(url, "uploadType", "resumable");
}

GURL AddMaxResultParam(const GURL& url, int max_results) {
  DCHECK_GT(max_results, 0);
  return net::AppendOrReplaceQueryParameter(
      url, "maxResults", base::IntToString(max_results));
}

}  // namespace

DriveApiUrlGenerator::DriveApiUrlGenerator(const GURL& base_url)
    : base_url_(base_url) {
  // Do nothing.
}

DriveApiUrlGenerator::~DriveApiUrlGenerator() {
  // Do nothing.
}

const char DriveApiUrlGenerator::kBaseUrlForProduction[] =
    "https://www.googleapis.com";

GURL DriveApiUrlGenerator::GetAboutUrl() const {
  return base_url_.Resolve(kDriveV2AboutUrl);
}

GURL DriveApiUrlGenerator::GetApplistUrl() const {
  return base_url_.Resolve(kDriveV2ApplistUrl);
}

GURL DriveApiUrlGenerator::GetChangelistUrl(
    bool include_deleted, int64 start_changestamp, int max_results) const {
  DCHECK_GE(start_changestamp, 0);

  GURL url = base_url_.Resolve(kDriveV2ChangelistUrl);
  if (!include_deleted) {
    // If include_deleted is set to "false", set the query parameter,
    // because its default parameter is "true".
    url = net::AppendOrReplaceQueryParameter(url, "includeDeleted", "false");
  }

  if (start_changestamp > 0) {
    url = net::AppendOrReplaceQueryParameter(
        url, "startChangeId", base::Int64ToString(start_changestamp));
  }

  return AddMaxResultParam(url, max_results);
}

GURL DriveApiUrlGenerator::GetFilesUrl() const {
  return base_url_.Resolve(kDriveV2FilesUrl);
}

GURL DriveApiUrlGenerator::GetFilelistUrl(
    const std::string& search_string, int max_results) const {
  GURL url = base_url_.Resolve(kDriveV2FilesUrl);
  url = AddMaxResultParam(url, max_results);
  return search_string.empty() ?
      url :
      net::AppendOrReplaceQueryParameter(url, "q", search_string);
}

GURL DriveApiUrlGenerator::GetFileUrl(const std::string& file_id) const {
  return base_url_.Resolve(kDriveV2FileUrlPrefix + net::EscapePath(file_id));
}

GURL DriveApiUrlGenerator::GetFileCopyUrl(
    const std::string& resource_id) const {
  return base_url_.Resolve(
      base::StringPrintf(kDriveV2FileCopyUrlFormat,
                         net::EscapePath(resource_id).c_str()));
}

GURL DriveApiUrlGenerator::GetFileTrashUrl(const std::string& file_id) const {
  return base_url_.Resolve(
      base::StringPrintf(kDriveV2FileTrashUrlFormat,
                         net::EscapePath(file_id).c_str()));
}

GURL DriveApiUrlGenerator::GetChildrenUrl(
    const std::string& resource_id) const {
  return base_url_.Resolve(
      base::StringPrintf(kDriveV2ChildrenUrlFormat,
                         net::EscapePath(resource_id).c_str()));
}

GURL DriveApiUrlGenerator::GetChildrenUrlForRemoval(
    const std::string& folder_id, const std::string& child_id) const {
  return base_url_.Resolve(
      base::StringPrintf(kDriveV2ChildrenUrlForRemovalFormat,
                         net::EscapePath(folder_id).c_str(),
                         net::EscapePath(child_id).c_str()));
}

GURL DriveApiUrlGenerator::GetInitiateUploadNewFileUrl() const {
  return AddResumableUploadParam(
      base_url_.Resolve(kDriveV2InitiateUploadNewFileUrl));
}

GURL DriveApiUrlGenerator::GetInitiateUploadExistingFileUrl(
    const std::string& resource_id) const {
  const GURL& url = base_url_.Resolve(
      kDriveV2InitiateUploadExistingFileUrlPrefix +
      net::EscapePath(resource_id));
  return AddResumableUploadParam(url);
}

}  // namespace google_apis
