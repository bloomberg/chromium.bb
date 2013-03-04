// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_TEST_UTIL_H_
#define CHROME_BROWSER_GOOGLE_APIS_TEST_UTIL_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class GURL;

namespace base {
class FilePath;
class Value;
}

namespace google_apis {

class AboutResource;
class AccountMetadata;
class AppList;
class AuthenticatedOperationInterface;
class ResourceEntry;
class ResourceList;
struct UploadRangeResponse;

namespace test_server {
struct HttpRequest;
class HttpResponse;
}

namespace test_util {

// Runs a task posted to the blocking pool, including subsequent tasks posted
// to the UI message loop and the blocking pool.
//
// A task is often posted to the blocking pool with PostTaskAndReply(). In
// that case, a task is posted back to the UI message loop, which can again
// post a task to the blocking pool. This function processes these tasks
// repeatedly.
void RunBlockingPoolTask();

// Removes |prefix| from |input| and stores the result in |output|. Returns
// true if the prefix is removed.
bool RemovePrefix(const std::string& input,
                  const std::string& prefix,
                  std::string* output);

// Returns the absolute path for a test file stored under
// chrome/test/data/chromeos.
base::FilePath GetTestFilePath(const std::string& relative_path);

// Returns the base URL for communicating with the local test server for
// testing, running at the specified port number.
GURL GetBaseUrlForTesting(int port);

// Loads a test JSON file as a base::Value, from a test file stored under
// chrome/test/data/chromeos.
scoped_ptr<base::Value> LoadJSONFile(const std::string& relative_path);

// Copies the results from EntryActionCallback.
void CopyResultsFromEntryActionCallback(GDataErrorCode* error_out,
                                        GDataErrorCode error_in);

// Copies the result from EntryActionCallback and quit the message loop.
void CopyResultFromEntryActionCallbackAndQuit(GDataErrorCode* error_out,
                                              GDataErrorCode error_in);

// Copies the results from GetDataCallback.
void CopyResultsFromGetDataCallback(GDataErrorCode* error_out,
                                    scoped_ptr<base::Value>* value_out,
                                    GDataErrorCode error_in,
                                    scoped_ptr<base::Value> value_in);

// Copies the results from GetDataCallback and quit the message loop.
void CopyResultsFromGetDataCallbackAndQuit(GDataErrorCode* error_out,
                                           scoped_ptr<base::Value>* value_out,
                                           GDataErrorCode error_in,
                                           scoped_ptr<base::Value> value_in);

// Copies the results from GetResourceEntryCallback.
void CopyResultsFromGetResourceEntryCallback(
    GDataErrorCode* error_out,
    scoped_ptr<ResourceEntry>* resource_entry_out,
    GDataErrorCode error_in,
    scoped_ptr<ResourceEntry> resource_entry_in);

// Copies the results from GetResourceListCallback.
void CopyResultsFromGetResourceListCallback(
    GDataErrorCode* error_out,
    scoped_ptr<ResourceList>* resource_list_out,
    GDataErrorCode error_in,
    scoped_ptr<ResourceList> resource_list_in);

// Copies the results from GetAccountMetadataCallback.
void CopyResultsFromGetAccountMetadataCallback(
    GDataErrorCode* error_out,
    scoped_ptr<AccountMetadata>* account_metadata_out,
    GDataErrorCode error_in,
    scoped_ptr<AccountMetadata> account_metadata_in);

// Copies the results from GetAccountMetadataCallback and quit the message
// loop.
void CopyResultsFromGetAccountMetadataCallbackAndQuit(
    GDataErrorCode* error_out,
    scoped_ptr<AccountMetadata>* account_metadata_out,
    GDataErrorCode error_in,
    scoped_ptr<AccountMetadata> account_metadata_in);

// Copies the results from GetAboutResourceCallback.
void CopyResultsFromGetAboutResourceCallback(
    GDataErrorCode* error_out,
    scoped_ptr<AboutResource>* about_resource_out,
    GDataErrorCode error_in,
    scoped_ptr<AboutResource> about_resource_in);

// Copies the results from GetAppListCallback.
void CopyResultsFromGetAppListCallback(
    GDataErrorCode* error_out,
    scoped_ptr<AppList>* app_list_out,
    GDataErrorCode error_in,
    scoped_ptr<AppList> app_list_in);

// Copies the results from DownloadActionCallback.
void CopyResultsFromDownloadActionCallback(
    GDataErrorCode* error_out,
    base::FilePath* temp_file_out,
    GDataErrorCode error_in,
    const base::FilePath& temp_file_in);

// Copies the results from InitiateUploadCallback.
void CopyResultsFromInitiateUploadCallback(
    GDataErrorCode* error_out,
    GURL* url_out,
    GDataErrorCode error_in,
    const GURL& url_in);

// Copies the results from InitiateUploadCallback and quit the message loop.
void CopyResultsFromInitiateUploadCallbackAndQuit(
    GDataErrorCode* error_out,
    GURL* url_out,
    GDataErrorCode error_in,
    const GURL& url_in);

// Copies the results from ResumeUploadCallback.
void CopyResultsFromUploadRangeCallback(
    UploadRangeResponse* response_out,
    scoped_ptr<ResourceEntry>* entry_out,
    const UploadRangeResponse& response_in,
    scoped_ptr<ResourceEntry> entry_in);

// Returns a HttpResponse created from the given file path.
scoped_ptr<test_server::HttpResponse> CreateHttpResponseFromFile(
    const base::FilePath& file_path);

// Does nothing for ReAuthenticateCallback(). This function should be used
// if it is not expected to reach this method as there won't be any
// authentication failures in the test.
void DoNothingForReAuthenticateCallback(
    AuthenticatedOperationInterface* operation);

// Handles a request for downloading a file. Reads a file from the test
// directory and returns the content. Also, copies the |request| to the memory
// pointed by |out_request|.
// |base_url| must be set to the server's base url.
scoped_ptr<test_server::HttpResponse> HandleDownloadRequest(
    const GURL& base_url,
    test_server::HttpRequest* out_request,
    const test_server::HttpRequest& request);

// Returns true if |json_data| is not NULL and equals to the content in
// |expected_json_file_path|. The failure reason will be logged into LOG(ERROR)
// if necessary.
bool VerifyJsonData(const base::FilePath& expected_json_file_path,
                    const base::Value* json_data);

// Parses a value of Content-Range header, which looks like
// "bytes <start_position>-<end_position>/<length>".
// Returns true on success.
bool ParseContentRangeHeader(const std::string& value,
                             int64* start_position,
                             int64* end_position,
                             int64* length);

}  // namespace test_util
}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_TEST_UTIL_H_
