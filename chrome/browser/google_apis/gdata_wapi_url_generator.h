// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// URL utility functions for Google Documents List API (aka WAPI).

#ifndef CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_URL_GENERATOR_H_
#define CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_URL_GENERATOR_H_

#include <string>

#include "googleurl/src/gurl.h"

namespace google_apis {

// The class is used to generate URLs for communicating with the WAPI server.
// for production, and the local server for testing.
class GDataWapiUrlGenerator {
 public:
  explicit GDataWapiUrlGenerator(const GURL& base_url);
  ~GDataWapiUrlGenerator();

  // The base URL for communicating with the WAPI server for production.
  static const char kBaseUrlForProduction[];

  // Adds additional parameters for API version, output content type and to
  // show folders in the feed are added to document feed URLs.
  static GURL AddStandardUrlParams(const GURL& url);

  // Adds additional parameters for initiate uploading as well as standard
  // url params (as AddStandardUrlParams above does).
  static GURL AddInitiateUploadUrlParams(const GURL& url);

  // Adds additional parameters for API version, output content type and to
  // show folders in the feed are added to document feed URLs.
  // Optionally, adds start-index=... parameter if |changestamp| is non-zero,
  // and adds q=... parameter if |search_string| is non-empty.
  static GURL AddFeedUrlParams(const GURL& url,
                               int num_items_to_fetch,
                               int changestamp,
                               const std::string& search_string);

  // Generates a URL for getting the resource list feed.
  //
  // The parameters other than |search_string| are mutually exclusive.
  // If |override_url| is non-empty, other parameters are ignored. Or if
  // |override_url| is empty and |shared_with_me| is true, others are not used.
  // Besides, |search_string| cannot be set together with |start_changestamp|.
  //
  // TODO(kinaba,haruki): http://crbug.com/160932
  // This is really hard to follow. We should split to multiple functions.
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
  GURL GenerateResourceListUrl(
      const GURL& override_url,
      int start_changestamp,
      const std::string& search_string,
      bool shared_with_me,
      const std::string& directory_resource_id) const;

  // Generates a URL for getting or editing the resource entry of
  // the given resource ID.
  GURL GenerateEditUrl(const std::string& resource_id) const;

  // Generates a URL for getting or editing the resource entry of the
  // given resource ID without query params.
  // Note that, in order to access to the WAPI server, it is necessary to
  // append some query parameters to the URL. GenerateEditUrl declared above
  // should be used in such cases. This method is designed for constructing
  // the data, such as xml element/attributes in request body containing
  // edit urls.
  GURL GenerateEditUrlWithoutParams(const std::string& resource_id) const;

  // Generates a URL for editing the contents in the directory specified
  // by the given resource ID.
  GURL GenerateContentUrl(const std::string& resource_id) const;

  // Generates a URL to remove an entry specified by |resource_id| from
  // the directory specified by the given |parent_resource_id|.
  GURL GenerateResourceUrlForRemoval(const std::string& parent_resource_id,
                                     const std::string& resource_id) const;

  // Generates a URL to initiate uploading a new file to a directory
  // specified by |parent_resource_id|.
  GURL GenerateInitiateUploadNewFileUrl(
      const std::string& parent_resource_id) const;

  // Generates a URL to initiate uploading file content to overwrite a
  // file specified by |resource_id|.
  GURL GenerateInitiateUploadExistingFileUrl(
      const std::string& resource_id) const;

  // Generates a URL for getting the root resource list feed.
  // Used to make changes in the root directory (ex. create a directory in the
  // root directory)
  GURL GenerateResourceListRootUrl() const;

  // Generates a URL for getting the account metadata feed.
  // If |include_installed_apps| is set to true, the response will include the
  // list of installed third party applications.
  GURL GenerateAccountMetadataUrl(bool include_installed_apps) const;

 private:
  const GURL base_url_;
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_URL_GENERATOR_H_
