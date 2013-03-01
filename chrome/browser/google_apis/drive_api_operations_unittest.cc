// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/google_apis/drive_api_operations.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/drive_api_url_generator.h"
#include "chrome/browser/google_apis/operation_registry.h"
#include "chrome/browser/google_apis/test_server/http_request.h"
#include "chrome/browser/google_apis/test_server/http_response.h"
#include "chrome/browser/google_apis/test_server/http_server.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/test/test_browser_thread.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kTestDriveApiAuthToken[] = "testtoken";
const char kTestUserAgent[] = "test-user-agent";

const char kTestChildrenResponse[] =
    "{\n"
    "\"kind\": \"drive#childReference\",\n"
    "\"id\": \"resource_id\",\n"
    "\"selfLink\": \"self_link\",\n"
    "\"childLink\": \"child_link\",\n"
    "}\n";

const char kTestUploadUrl[] = "https://server/upload/path";

void CopyResultsFromGetAboutResourceCallbackAndQuit(
    GDataErrorCode* error_out,
    scoped_ptr<AboutResource>* about_resource_out,
    const GDataErrorCode error_in,
    scoped_ptr<AboutResource> about_resource_in) {
  *error_out = error_in;
  *about_resource_out = about_resource_in.Pass();
  MessageLoop::current()->Quit();
}

void CopyResultsFromFileResourceCallbackAndQuit(
    GDataErrorCode* error_out,
    scoped_ptr<FileResource>* file_resource_out,
    const GDataErrorCode error_in,
    scoped_ptr<FileResource> file_resource_in) {
  *error_out = error_in;
  *file_resource_out = file_resource_in.Pass();
  MessageLoop::current()->Quit();
}

}  // namespace

class DriveApiOperationsTest : public testing::Test {
 public:
  DriveApiOperationsTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE),
        io_thread_(content::BrowserThread::IO) {
  }

  virtual void SetUp() OVERRIDE {
    file_thread_.Start();
    io_thread_.StartIOThread();

    request_context_getter_ = new net::TestURLRequestContextGetter(
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::IO));

    ASSERT_TRUE(test_server_.InitializeAndWaitUntilReady());
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiOperationsTest::HandleChildrenDeleteRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiOperationsTest::HandleDataFileRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiOperationsTest::HandleInitiateUploadRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiOperationsTest::HandleContentResponse,
                   base::Unretained(this)));

    url_generator_.reset(new DriveApiUrlGenerator(
        test_util::GetBaseUrlForTesting(test_server_.port())));

    // Reset the server's expected behavior just in case.
    ResetExpectedResponse();
  }

  virtual void TearDown() OVERRIDE {
    test_server_.ShutdownAndWaitUntilComplete();
    request_context_getter_ = NULL;
    ResetExpectedResponse();
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;
  test_server::HttpServer test_server_;
  OperationRegistry operation_registry_;
  scoped_ptr<DriveApiUrlGenerator> url_generator_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;

  // This is a path to the file which contains expected response from
  // the server. See also HandleDataFileRequest below.
  base::FilePath expected_data_file_path_;

  // This is a url string in the expected response header from the server
  // for initiating file uploading.
  std::string expected_upload_url_;

  // These are content and its type in the expected response from the server.
  // See also HandleContentResponse below.
  std::string expected_content_type_;
  std::string expected_content_;

  // The incoming HTTP request is saved so tests can verify the request
  // parameters like HTTP method (ex. some operations should use DELETE
  // instead of GET).
  test_server::HttpRequest http_request_;

 private:
  void ResetExpectedResponse() {
    expected_data_file_path_.clear();
    expected_upload_url_.clear();
    expected_content_type_.clear();
    expected_content_.clear();
  }

  // For "Children: delete" request, the server will return "204 No Content"
  // response meaning "success".
  scoped_ptr<test_server::HttpResponse> HandleChildrenDeleteRequest(
      const test_server::HttpRequest& request) {
    if (request.method != test_server::METHOD_DELETE ||
        request.relative_url.find("/children/") == string::npos) {
      // The request is not the "Children: delete" operation. Delegate the
      // processing to the next handler.
      return scoped_ptr<test_server::HttpResponse>();
    }

    http_request_ = request;

    // Return the response with just "204 No Content" status code.
    scoped_ptr<test_server::HttpResponse> http_response(
        new test_server::HttpResponse);
    http_response->set_code(test_server::NO_CONTENT);
    return http_response.Pass();
  }

  // Reads the data file of |expected_data_file_path_| and returns its content
  // for the request.
  // To use this method, it is necessary to set |expected_data_file_path_|
  // to the appropriate file path before sending the request to the server.
  scoped_ptr<test_server::HttpResponse> HandleDataFileRequest(
      const test_server::HttpRequest& request) {
    if (expected_data_file_path_.empty()) {
      // The file is not specified. Delegate the processing to the next
      // handler.
      return scoped_ptr<test_server::HttpResponse>();
    }

    http_request_ = request;

    // Return the response from the data file.
    return test_util::CreateHttpResponseFromFile(expected_data_file_path_);
  }

  // Returns the response based on set expected upload url.
  // The response contains the url in its "Location: " header. Also, it doesn't
  // have any content.
  // To use this method, it is necessary to set |expected_upload_url_|
  // to the string representation of the url to be returned.
  scoped_ptr<test_server::HttpResponse> HandleInitiateUploadRequest(
      const test_server::HttpRequest& request) {
    if (expected_upload_url_.empty()) {
      // Expected upload url is not set. Delegate the processing to the next
      // handler.
      return scoped_ptr<test_server::HttpResponse>();
    }

    http_request_ = request;

    scoped_ptr<test_server::HttpResponse> response(
        new test_server::HttpResponse);
    response->set_code(test_server::SUCCESS);
    response->AddCustomHeader("Location", expected_upload_url_);
    return response.Pass();
  }

  // Returns the response based on set expected content and its type.
  // To use this method, both |expected_content_type_| and |expected_content_|
  // must be set in advance.
  scoped_ptr<test_server::HttpResponse> HandleContentResponse(
      const test_server::HttpRequest& request) {
    if (expected_content_type_.empty() || expected_content_.empty()) {
      // Expected content is not set. Delegate the processing to the next
      // handler.
      return scoped_ptr<test_server::HttpResponse>();
    }

    http_request_ = request;

    scoped_ptr<test_server::HttpResponse> response(
        new test_server::HttpResponse);
    response->set_code(test_server::SUCCESS);
    response->set_content_type(expected_content_type_);
    response->set_content(expected_content_);
    return response.Pass();
  }
};

TEST_F(DriveApiOperationsTest, GetAboutOperation_ValidFeed) {
  // Set an expected data file containing valid result.
  expected_data_file_path_ = test_util::GetTestFilePath("drive/about.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AboutResource> feed_data;

  GetAboutOperation* operation = new GetAboutOperation(
      &operation_registry_,
      request_context_getter_.get(),
      *url_generator_,
      base::Bind(&CopyResultsFromGetAboutResourceCallbackAndQuit,
                 &error, &feed_data));
  operation->Start(kTestDriveApiAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/drive/v2/about", http_request_.relative_url);

  scoped_ptr<AboutResource> expected(
      AboutResource::CreateFrom(*test_util::LoadJSONFile("drive/about.json")));
  ASSERT_TRUE(feed_data.get());
  EXPECT_EQ(expected->largest_change_id(), feed_data->largest_change_id());
  EXPECT_EQ(expected->quota_bytes_total(), feed_data->quota_bytes_total());
  EXPECT_EQ(expected->quota_bytes_used(), feed_data->quota_bytes_used());
  EXPECT_EQ(expected->root_folder_id(), feed_data->root_folder_id());
}

TEST_F(DriveApiOperationsTest, GetAboutOperation_InvalidFeed) {
  // Set an expected data file containing invalid result.
  expected_data_file_path_ = test_util::GetTestFilePath("gdata/testfile.txt");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AboutResource> feed_data;

  GetAboutOperation* operation = new GetAboutOperation(
      &operation_registry_,
      request_context_getter_.get(),
      *url_generator_,
      base::Bind(&CopyResultsFromGetAboutResourceCallbackAndQuit,
                 &error, &feed_data));
  operation->Start(kTestDriveApiAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  // "parse error" should be returned, and the feed should be NULL.
  EXPECT_EQ(GDATA_PARSE_ERROR, error);
  EXPECT_EQ(test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/drive/v2/about", http_request_.relative_url);
  EXPECT_FALSE(feed_data.get());
}

TEST_F(DriveApiOperationsTest, CreateDirectoryOperation) {
  // Set an expected data file containing the directory's entry data.
  expected_data_file_path_ =
      test_util::GetTestFilePath("drive/directory_entry.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<FileResource> feed_data;

  // Create "new directory" in the root directory.
  drive::CreateDirectoryOperation* operation =
      new drive::CreateDirectoryOperation(
          &operation_registry_,
          request_context_getter_.get(),
          *url_generator_,
          "root",
          "new directory",
          base::Bind(&CopyResultsFromFileResourceCallbackAndQuit,
                     &error, &feed_data));
  operation->Start(kTestDriveApiAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/drive/v2/files", http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);

  EXPECT_TRUE(http_request_.has_content);

  scoped_ptr<FileResource> expected(
      FileResource::CreateFrom(
          *test_util::LoadJSONFile("drive/directory_entry.json")));

  // Sanity check.
  ASSERT_TRUE(feed_data.get());

  EXPECT_EQ(expected->file_id(), feed_data->file_id());
  EXPECT_EQ(expected->title(), feed_data->title());
  EXPECT_EQ(expected->mime_type(), feed_data->mime_type());
  EXPECT_EQ(expected->parents().size(), feed_data->parents().size());
}

TEST_F(DriveApiOperationsTest, RenameResourceOperation) {
  // Set an expected data file containing the directory's entry data.
  // It'd be returned if we rename a directory.
  expected_data_file_path_ =
      test_util::GetTestFilePath("drive/directory_entry.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;

  // Create "new directory" in the root directory.
  drive::RenameResourceOperation* operation =
      new drive::RenameResourceOperation(
          &operation_registry_,
          request_context_getter_.get(),
          *url_generator_,
          "resource_id",
          "new name",
          base::Bind(&test_util::CopyResultFromEntryActionCallbackAndQuit,
                     &error));
  operation->Start(kTestDriveApiAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(test_server::METHOD_PATCH, http_request_.method);
  EXPECT_EQ("/drive/v2/files/resource_id", http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"title\":\"new name\"}", http_request_.content);
}

TEST_F(DriveApiOperationsTest, TrashResourceOperation) {
  // Set data for the expected result. Directory entry should be returned
  // if the trashing entry is a directory, so using it here should be fine.
  expected_data_file_path_ =
      test_util::GetTestFilePath("drive/directory_entry.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;

  // Trash a resource with the given resource id.
  drive::TrashResourceOperation* operation =
      new drive::TrashResourceOperation(
          &operation_registry_,
          request_context_getter_.get(),
          *url_generator_,
          "resource_id",
          base::Bind(&test_util::CopyResultFromEntryActionCallbackAndQuit,
                     &error));
  operation->Start(kTestDriveApiAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/drive/v2/files/resource_id/trash", http_request_.relative_url);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_TRUE(http_request_.content.empty());
}

TEST_F(DriveApiOperationsTest, InsertResourceOperation) {
  // Set an expected data file containing the children entry.
  expected_content_type_ = "application/json";
  expected_content_ = kTestChildrenResponse;

  GDataErrorCode error = GDATA_OTHER_ERROR;

  // Add a resource with "resource_id" to a directory with
  // "parent_resource_id".
  drive::InsertResourceOperation* operation =
      new drive::InsertResourceOperation(
          &operation_registry_,
          request_context_getter_.get(),
          *url_generator_,
          "parent_resource_id",
          "resource_id",
          base::Bind(&test_util::CopyResultFromEntryActionCallbackAndQuit,
                     &error));
  operation->Start(kTestDriveApiAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/drive/v2/files/parent_resource_id/children",
            http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"id\":\"resource_id\"}", http_request_.content);
}

TEST_F(DriveApiOperationsTest, DeleteResourceOperation) {
  GDataErrorCode error = GDATA_OTHER_ERROR;

  // Remove a resource with "resource_id" from a directory with
  // "parent_resource_id".
  drive::DeleteResourceOperation* operation =
      new drive::DeleteResourceOperation(
          &operation_registry_,
          request_context_getter_.get(),
          *url_generator_,
          "parent_resource_id",
          "resource_id",
          base::Bind(&test_util::CopyResultFromEntryActionCallbackAndQuit,
                     &error));
  operation->Start(kTestDriveApiAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_NO_CONTENT, error);
  EXPECT_EQ(test_server::METHOD_DELETE, http_request_.method);
  EXPECT_EQ("/drive/v2/files/parent_resource_id/children/resource_id",
            http_request_.relative_url);
  EXPECT_FALSE(http_request_.has_content);
}

TEST_F(DriveApiOperationsTest, InitiateUploadNewFileOperation) {
  // Set an expected url for uploading.
  expected_upload_url_ = kTestUploadUrl;

  const char kTestContentType[] = "text/plain";
  const int64 kTestContentLength = 100;

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  drive::InitiateUploadNewFileOperation* operation =
      new drive::InitiateUploadNewFileOperation(
          &operation_registry_,
          request_context_getter_.get(),
          *url_generator_,
          base::FilePath(FILE_PATH_LITERAL("drive/file/path")),
          kTestContentType,
          kTestContentLength,
          "parent_resource_id",  // The resource id of the parent directory.
          "new file title",  // The title of the file being uploaded.
          base::Bind(&test_util::CopyResultsFromInitiateUploadCallbackAndQuit,
                     &error, &url));
  operation->Start(kTestDriveApiAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadUrl, url.spec());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kTestContentLength),
            http_request_.headers["X-Upload-Content-Length"]);

  EXPECT_EQ(test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/upload/drive/v2/files?uploadType=resumable",
            http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"parents\":[{"
                "\"id\":\"parent_resource_id\","
                "\"kind\":\"drive#fileLink\""
            "}],"
            "\"title\":\"new file title\"}",
            http_request_.content);

  // Clean the operation remaining in |operation_registry_|.
  operation_registry_.CancelAll();
}

}  // namespace google_apis
