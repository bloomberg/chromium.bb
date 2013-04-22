// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_URL_GENERATOR_H_
#define CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_URL_GENERATOR_H_

#include <string>

#include "googleurl/src/gurl.h"

namespace google_apis {

// This class is used to generate URLs for communicating with drive api
// servers for production, and a local server for testing.
class DriveApiUrlGenerator {
 public:
  // |base_url| is the path to the target drive api server.
  // Note that this is an injecting point for a testing server.
  explicit DriveApiUrlGenerator(const GURL& base_url);
  ~DriveApiUrlGenerator();

  // The base URL for communicating with the production drive api server.
  static const char kBaseUrlForProduction[];

  // Returns a URL to fetch "about" data.
  GURL GetAboutUrl() const;

  // Returns a URL to fetch "applist" data.
  GURL GetApplistUrl() const;

  // Returns a URL to fetch a list of changes.
  // include_deleted:
  //   Set to true if the requesting change list should contain the deleted
  //   entries. Otherwise false.
  // start_changestamp:
  //   The starting point of the requesting change list, or 0 if all changes
  //   are necessary.
  // max_results:
  //   The max of the number of files resource in the response.
  GURL GetChangelistUrl(
      bool include_deleted, int64 start_changestamp, int max_results) const;

  // Returns a URL to edit (especially add) a resource, such as inserting
  // a file metadata or creating a new directory.
  GURL GetFilesUrl() const;

  // Returns a URL to fetch a list of files with the given |search_string|.
  // search_string: The search query.
  // max_results: The max of the number of files resource in the response.
  GURL GetFilelistUrl(const std::string& search_string, int max_results) const;

  // Returns a URL to fetch a file content.
  GURL GetFileUrl(const std::string& file_id) const;

  // Returns a URL to copy a file specified by |resource_id|.
  GURL GetFileCopyUrl(const std::string& resource_id) const;

  // Returns a URL to trash a resource with the given |resource_id|.
  // Note that the |resource_id| is corresponding to the "file id" in the
  // document: https://developers.google.com/drive/v2/reference/files/trash
  // but we use the term "resource" for consistency in our code.
  GURL GetFileTrashUrl(const std::string& resource_id) const;

  // Returns a URL to add a resource to a directory with |resource_id|.
  // Note that the |resource_id| is corresponding to the "folder id" in the
  // document: https://developers.google.com/drive/v2/reference/children/insert
  // but we use the term "resource" for consistency in our code.
  GURL GetChildrenUrl(const std::string& resource_id) const;

  // Returns a URL to remove a resource with |child_id| from a directory
  // with |folder_id|.
  // Note that we use the name "folder" for the parameter, in order to be
  // consistent with the drive API document:
  // https://developers.google.com/drive/v2/reference/children/delete
  GURL GetChildrenUrlForRemoval(const std::string& folder_id,
                                const std::string& child_id) const;

  // Returns a URL to initiate uploading a new file.
  GURL GetInitiateUploadNewFileUrl() const;

  // Returns a URL to initiate uploading an existing file specified by
  // |resource_id|.
  GURL GetInitiateUploadExistingFileUrl(const std::string& resource_id) const;

 private:
  const GURL base_url_;

  // This class is copyable hence no DISALLOW_COPY_AND_ASSIGN here.
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_URL_GENERATOR_H_
