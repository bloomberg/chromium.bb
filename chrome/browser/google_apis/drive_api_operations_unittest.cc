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
const char kTestETag[] = "test_etag";
const char kTestUserAgent[] = "test-user-agent";

const char kTestChildrenResponse[] =
    "{\n"
    "\"kind\": \"drive#childReference\",\n"
    "\"id\": \"resource_id\",\n"
    "\"selfLink\": \"self_link\",\n"
    "\"childLink\": \"child_link\",\n"
    "}\n";

const char kTestUploadExistingFilePath[] = "/upload/existingfile/path";
const char kTestUploadNewFilePath[] = "/upload/newfile/path";

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

void CopyResultFromUploadRangeCallbackAndQuit(
    UploadRangeResponse* response_out,
    scoped_ptr<FileResource>* file_resource_out,
    const UploadRangeResponse& response_in,
    scoped_ptr<FileResource> file_resource_in) {
  *response_out = response_in;
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
        base::Bind(&DriveApiOperationsTest::HandleResumeUploadRequest,
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
    received_bytes_ = 0;
    content_length_ = 0;
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

  // This is a path string in the expected response header from the server
  // for initiating file uploading.
  std::string expected_upload_path_;

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
    expected_upload_path_.clear();
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
  // To use this method, it is necessary to set |expected_upload_path_|
  // to the string representation of the url to be returned.
  scoped_ptr<test_server::HttpResponse> HandleInitiateUploadRequest(
      const test_server::HttpRequest& request) {
    if (request.relative_url == expected_upload_path_ ||
        expected_upload_path_.empty()) {
      // The request is for resume uploading or the expected upload url is not
      // set. Delegate the processing to the next handler.
      return scoped_ptr<test_server::HttpResponse>();
    }

    http_request_ = request;

    scoped_ptr<test_server::HttpResponse> response(
        new test_server::HttpResponse);

    // Check an ETag.
    std::map<std::string, std::string>::const_iterator found =
        request.headers.find("If-Match");
    if (found != request.headers.end() &&
        found->second != "*" &&
        found->second != kTestETag) {
      response->set_code(test_server::PRECONDITION);
      return response.Pass();
    }

    // Check if the X-Upload-Content-Length is present. If yes, store the
    // length of the file.
    found = request.headers.find("X-Upload-Content-Length");
    if (found == request.headers.end() ||
        !base::StringToInt64(found->second, &content_length_)) {
      return scoped_ptr<test_server::HttpResponse>();
    }
    received_bytes_ = 0;

    response->set_code(test_server::SUCCESS);
    response->AddCustomHeader(
        "Location",
        test_server_.base_url().Resolve(expected_upload_path_).spec());
    return response.Pass();
  }

  scoped_ptr<test_server::HttpResponse> HandleResumeUploadRequest(
      const test_server::HttpRequest& request) {
    if (request.relative_url != expected_upload_path_) {
      // The request path is different from the expected path for uploading.
      // Delegate the processing to the next handler.
      return scoped_ptr<test_server::HttpResponse>();
    }

    http_request_ = request;

    if (!request.content.empty()) {
      std::map<std::string, std::string>::const_iterator iter =
          request.headers.find("Content-Range");
      if (iter == request.headers.end()) {
        // The range must be set.
        return scoped_ptr<test_server::HttpResponse>();
      }

      int64 length = 0;
      int64 start_position = 0;
      int64 end_position = 0;
      if (!test_util::ParseContentRangeHeader(
              iter->second, &start_position, &end_position, &length)) {
        // Invalid "Content-Range" value.
        return scoped_ptr<test_server::HttpResponse>();
      }

      EXPECT_EQ(start_position, received_bytes_);
      EXPECT_EQ(length, content_length_);

      // end_position is inclusive, but so +1 to change the range to byte size.
      received_bytes_ = end_position + 1;
    }

    if (received_bytes_ < content_length_) {
      scoped_ptr<test_server::HttpResponse> response(
          new test_server::HttpResponse);
      // Set RESUME INCOMPLETE (308) status code.
      response->set_code(test_server::RESUME_INCOMPLETE);

      // Add Range header to the response, based on the values of
      // Content-Range header in the request.
      // The header is annotated only when at least one byte is received.
      if (received_bytes_ > 0) {
        response->AddCustomHeader(
            "Range", "bytes=0-" + base::Int64ToString(received_bytes_ - 1));
      }

      return response.Pass();
    }

    // All bytes are received. Return the "success" response with the file's
    // (dummy) metadata.
    scoped_ptr<test_server::HttpResponse> response =
        test_util::CreateHttpResponseFromFile(
            test_util::GetTestFilePath("drive/file_entry.json"));

    // The response code is CREATED if it is new file uploading.
    if (http_request_.relative_url == kTestUploadNewFilePath) {
      response->set_code(test_server::CREATED);
    }

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

  // These are for the current upload file status.
  int64 received_bytes_;
  int64 content_length_;
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

TEST_F(DriveApiOperationsTest, UploadNewFileOperation) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadNewFilePath;

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_url;

  // 1) Initiate uploading a new file to the directory with
  // "parent_resource_id".
  drive::InitiateUploadNewFileOperation* operation =
      new drive::InitiateUploadNewFileOperation(
          &operation_registry_,
          request_context_getter_.get(),
          *url_generator_,
          base::FilePath(FILE_PATH_LITERAL("drive/file/path")),
          kTestContentType,
          kTestContent.size(),
          "parent_resource_id",  // The resource id of the parent directory.
          "new file title",  // The title of the file being uploaded.
          base::Bind(&test_util::CopyResultsFromInitiateUploadCallbackAndQuit,
                     &error, &upload_url));
  operation->Start(kTestDriveApiAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadNewFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kTestContent.size()),
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

  // 2) Upload the content to the upload URL.
  scoped_refptr<net::IOBuffer> buffer = new net::StringIOBuffer(kTestContent);

  UploadRangeResponse response;
  scoped_ptr<FileResource> new_entry;

  drive::ResumeUploadOperation* resume_operation =
      new drive::ResumeUploadOperation(
          &operation_registry_,
          request_context_getter_.get(),
          UPLOAD_NEW_FILE,
          base::FilePath(FILE_PATH_LITERAL("drive/file/path")),
          upload_url,
          0,  // start_position
          kTestContent.size(),  // end_position (exclusive)
          kTestContent.size(),  // content_length,
          kTestContentType,
          buffer,
          base::Bind(&CopyResultFromUploadRangeCallbackAndQuit,
                     &response, &new_entry));

  resume_operation->Start(
      kTestDriveApiAuthToken, kTestUserAgent,
      base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should be added.
  EXPECT_EQ("bytes 0-" +
            base::Int64ToString(kTestContent.size() - 1) + "/" +
            base::Int64ToString(kTestContent.size()),
            http_request_.headers["Content-Range"]);
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kTestContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_CREATED, response.code);  // Because it's a new file
  // The start and end positions should be set to -1, if an upload is complete.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);
}

TEST_F(DriveApiOperationsTest, UploadNewEmptyFileOperation) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadNewFilePath;

  const char kTestContentType[] = "text/plain";
  const char kTestContent[] = "";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_url;

  // 1) Initiate uploading a new file to the directory with
  // "parent_resource_id".
  drive::InitiateUploadNewFileOperation* operation =
      new drive::InitiateUploadNewFileOperation(
          &operation_registry_,
          request_context_getter_.get(),
          *url_generator_,
          base::FilePath(FILE_PATH_LITERAL("drive/file/path")),
          kTestContentType,
          0,
          "parent_resource_id",  // The resource id of the parent directory.
          "new file title",  // The title of the file being uploaded.
          base::Bind(&test_util::CopyResultsFromInitiateUploadCallbackAndQuit,
                     &error, &upload_url));
  operation->Start(kTestDriveApiAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadNewFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ("0", http_request_.headers["X-Upload-Content-Length"]);

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

  // 2) Upload the content to the upload URL.
  scoped_refptr<net::IOBuffer> buffer = new net::StringIOBuffer(kTestContent);

  UploadRangeResponse response;
  scoped_ptr<FileResource> new_entry;

  drive::ResumeUploadOperation* resume_operation =
      new drive::ResumeUploadOperation(
          &operation_registry_,
          request_context_getter_.get(),
          UPLOAD_NEW_FILE,
          base::FilePath(FILE_PATH_LITERAL("drive/file/path")),
          upload_url,
          0,  // start_position
          0,  // end_position (exclusive)
          0,  // content_length,
          kTestContentType,
          buffer,
          base::Bind(&CopyResultFromUploadRangeCallbackAndQuit,
                     &response, &new_entry));

  resume_operation->Start(
      kTestDriveApiAuthToken, kTestUserAgent,
      base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should NOT be added.
  EXPECT_EQ(0U, http_request_.headers.count("Content-Range"));
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kTestContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_CREATED, response.code);  // Because it's a new file
  // The start and end positions should be set to -1, if an upload is complete.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);
}

TEST_F(DriveApiOperationsTest, UploadNewLargeFileOperation) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadNewFilePath;

  const char kTestContentType[] = "text/plain";
  const size_t kNumChunkBytes = 10;  // Num bytes in a chunk.
  const std::string kTestContent(100, 'a');

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_url;

  // 1) Initiate uploading a new file to the directory with
  // "parent_resource_id".
  drive::InitiateUploadNewFileOperation* operation =
      new drive::InitiateUploadNewFileOperation(
          &operation_registry_,
          request_context_getter_.get(),
          *url_generator_,
          base::FilePath(FILE_PATH_LITERAL("drive/file/path")),
          kTestContentType,
          kTestContent.size(),
          "parent_resource_id",  // The resource id of the parent directory.
          "new file title",  // The title of the file being uploaded.
          base::Bind(&test_util::CopyResultsFromInitiateUploadCallbackAndQuit,
                     &error, &upload_url));
  operation->Start(kTestDriveApiAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadNewFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kTestContent.size()),
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

  // 2) Upload the content to the upload URL.
  for (size_t start_position = 0; start_position < kTestContent.size();
       start_position += kNumChunkBytes) {
    const std::string payload = kTestContent.substr(
        start_position,
        std::min(kNumChunkBytes, kTestContent.size() - start_position));
    const size_t end_position = start_position + payload.size();
    scoped_refptr<net::IOBuffer> buffer = new net::StringIOBuffer(payload);

    UploadRangeResponse response;
    scoped_ptr<FileResource> new_entry;

    drive::ResumeUploadOperation* resume_operation =
        new drive::ResumeUploadOperation(
            &operation_registry_,
            request_context_getter_.get(),
            UPLOAD_NEW_FILE,
            base::FilePath(FILE_PATH_LITERAL("drive/file/path")),
            upload_url,
            start_position,
            end_position,
            kTestContent.size(),  // content_length,
            kTestContentType,
            buffer,
            base::Bind(&CopyResultFromUploadRangeCallbackAndQuit,
                       &response, &new_entry));

    resume_operation->Start(
        kTestDriveApiAuthToken, kTestUserAgent,
        base::Bind(&test_util::DoNothingForReAuthenticateCallback));
    MessageLoop::current()->Run();

    // METHOD_PUT should be used to upload data.
    EXPECT_EQ(test_server::METHOD_PUT, http_request_.method);
    // Request should go to the upload URL.
    EXPECT_EQ(upload_url.path(), http_request_.relative_url);
    // Content-Range header should be added.
    EXPECT_EQ("bytes " +
              base::Int64ToString(start_position) + "-" +
              base::Int64ToString(end_position - 1) + "/" +
              base::Int64ToString(kTestContent.size()),
              http_request_.headers["Content-Range"]);
    // The upload content should be set in the HTTP request.
    EXPECT_TRUE(http_request_.has_content);
    EXPECT_EQ(payload, http_request_.content);

    if (end_position == kTestContent.size()) {
      // Check the response.
      EXPECT_EQ(HTTP_CREATED, response.code);  // Because it's a new file
      // The start and end positions should be set to -1, if an upload is
      // complete.
      EXPECT_EQ(-1, response.start_position_received);
      EXPECT_EQ(-1, response.end_position_received);
    } else {
      // Check the response.
      EXPECT_EQ(HTTP_RESUME_INCOMPLETE, response.code);
      EXPECT_EQ(0, response.start_position_received);
      EXPECT_EQ(static_cast<int64>(end_position),
                response.end_position_received);
    }
  }
}

TEST_F(DriveApiOperationsTest, UploadExistingFileOperation) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadExistingFilePath;

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  drive::InitiateUploadExistingFileOperation* operation =
      new drive::InitiateUploadExistingFileOperation(
          &operation_registry_,
          request_context_getter_.get(),
          *url_generator_,
          base::FilePath(FILE_PATH_LITERAL("drive/file/path")),
          kTestContentType,
          kTestContent.size(),
          "resource_id",  // The resource id of the file to be overwritten.
          "",  // No etag.
          base::Bind(&test_util::CopyResultsFromInitiateUploadCallbackAndQuit,
                     &error, &upload_url));
  operation->Start(kTestDriveApiAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadExistingFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);
  EXPECT_EQ("*", http_request_.headers["If-Match"]);

  EXPECT_EQ(test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ("/upload/drive/v2/files/resource_id?uploadType=resumable",
            http_request_.relative_url);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_TRUE(http_request_.content.empty());

  // 2) Upload the content to the upload URL.
  scoped_refptr<net::IOBuffer> buffer = new net::StringIOBuffer(kTestContent);

  UploadRangeResponse response;
  scoped_ptr<FileResource> new_entry;

  drive::ResumeUploadOperation* resume_operation =
      new drive::ResumeUploadOperation(
          &operation_registry_,
          request_context_getter_.get(),
          UPLOAD_EXISTING_FILE,
          base::FilePath(FILE_PATH_LITERAL("drive/file/path")),
          upload_url,
          0,  // start_position
          kTestContent.size(),  // end_position (exclusive)
          kTestContent.size(),  // content_length,
          kTestContentType,
          buffer,
          base::Bind(&CopyResultFromUploadRangeCallbackAndQuit,
                     &response, &new_entry));

  resume_operation->Start(
      kTestDriveApiAuthToken, kTestUserAgent,
      base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should be added.
  EXPECT_EQ("bytes 0-" +
            base::Int64ToString(kTestContent.size() - 1) + "/" +
            base::Int64ToString(kTestContent.size()),
            http_request_.headers["Content-Range"]);
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kTestContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_SUCCESS, response.code);  // Because it's an existing file
  // The start and end positions should be set to -1, if an upload is complete.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);
}

TEST_F(DriveApiOperationsTest, UploadExistingFileOperationWithETag) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadExistingFilePath;

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  drive::InitiateUploadExistingFileOperation* operation =
      new drive::InitiateUploadExistingFileOperation(
          &operation_registry_,
          request_context_getter_.get(),
          *url_generator_,
          base::FilePath(FILE_PATH_LITERAL("drive/file/path")),
          kTestContentType,
          kTestContent.size(),
          "resource_id",  // The resource id of the file to be overwritten.
          kTestETag,
          base::Bind(&test_util::CopyResultsFromInitiateUploadCallbackAndQuit,
                     &error, &upload_url));
  operation->Start(kTestDriveApiAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadExistingFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);
  EXPECT_EQ(kTestETag, http_request_.headers["If-Match"]);

  EXPECT_EQ(test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ("/upload/drive/v2/files/resource_id?uploadType=resumable",
            http_request_.relative_url);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_TRUE(http_request_.content.empty());

  // 2) Upload the content to the upload URL.
  scoped_refptr<net::IOBuffer> buffer = new net::StringIOBuffer(kTestContent);

  UploadRangeResponse response;
  scoped_ptr<FileResource> new_entry;

  drive::ResumeUploadOperation* resume_operation =
      new drive::ResumeUploadOperation(
          &operation_registry_,
          request_context_getter_.get(),
          UPLOAD_EXISTING_FILE,
          base::FilePath(FILE_PATH_LITERAL("drive/file/path")),
          upload_url,
          0,  // start_position
          kTestContent.size(),  // end_position (exclusive)
          kTestContent.size(),  // content_length,
          kTestContentType,
          buffer,
          base::Bind(&CopyResultFromUploadRangeCallbackAndQuit,
                     &response, &new_entry));

  resume_operation->Start(
      kTestDriveApiAuthToken, kTestUserAgent,
      base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should be added.
  EXPECT_EQ("bytes 0-" +
            base::Int64ToString(kTestContent.size() - 1) + "/" +
            base::Int64ToString(kTestContent.size()),
            http_request_.headers["Content-Range"]);
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kTestContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_SUCCESS, response.code);  // Because it's an existing file
  // The start and end positions should be set to -1, if an upload is complete.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);
}

TEST_F(DriveApiOperationsTest, UploadExistingFileOperationWithETagConflicting) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadExistingFilePath;

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  drive::InitiateUploadExistingFileOperation* operation =
      new drive::InitiateUploadExistingFileOperation(
          &operation_registry_,
          request_context_getter_.get(),
          *url_generator_,
          base::FilePath(FILE_PATH_LITERAL("drive/file/path")),
          kTestContentType,
          kTestContent.size(),
          "resource_id",  // The resource id of the file to be overwritten.
          "Conflicting-etag",
          base::Bind(&test_util::CopyResultsFromInitiateUploadCallbackAndQuit,
                     &error, &upload_url));
  operation->Start(kTestDriveApiAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_PRECONDITION, error);
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);
  EXPECT_EQ("Conflicting-etag", http_request_.headers["If-Match"]);

  EXPECT_EQ(test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ("/upload/drive/v2/files/resource_id?uploadType=resumable",
            http_request_.relative_url);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_TRUE(http_request_.content.empty());
}

}  // namespace google_apis
