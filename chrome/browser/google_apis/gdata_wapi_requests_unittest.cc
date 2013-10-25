// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/google_apis/dummy_auth_service.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_requests.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"
#include "chrome/browser/google_apis/request_sender.h"
#include "chrome/browser/google_apis/test_util.h"
#include "net/base/escape.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kTestUserAgent[] = "test-user-agent";
const char kTestETag[] = "test_etag";
const char kTestDownloadPathPrefix[] = "/download/";

class GDataWapiRequestsTest : public testing::Test {
 public:
  GDataWapiRequestsTest() {
  }

  virtual void SetUp() OVERRIDE {
    request_context_getter_ = new net::TestURLRequestContextGetter(
        message_loop_.message_loop_proxy());

    request_sender_.reset(new RequestSender(new DummyAuthService,
                                            request_context_getter_.get(),
                                            message_loop_.message_loop_proxy(),
                                            kTestUserAgent));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(test_server_.InitializeAndWaitUntilReady());
    test_server_.RegisterRequestHandler(
        base::Bind(&test_util::HandleDownloadFileRequest,
                   test_server_.base_url(),
                   base::Unretained(&http_request_)));
    test_server_.RegisterRequestHandler(
        base::Bind(&GDataWapiRequestsTest::HandleResourceFeedRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&GDataWapiRequestsTest::HandleMetadataRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&GDataWapiRequestsTest::HandleCreateSessionRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&GDataWapiRequestsTest::HandleUploadRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&GDataWapiRequestsTest::HandleDownloadRequest,
                   base::Unretained(this)));

    GURL test_base_url = test_util::GetBaseUrlForTesting(test_server_.port());
    url_generator_.reset(new GDataWapiUrlGenerator(
        test_base_url, test_base_url.Resolve(kTestDownloadPathPrefix)));

    received_bytes_ = 0;
    content_length_ = 0;
  }

 protected:
  // Handles a request for fetching a resource feed.
  scoped_ptr<net::test_server::HttpResponse> HandleResourceFeedRequest(
      const net::test_server::HttpRequest& request) {
    http_request_ = request;

    const GURL absolute_url = test_server_.GetURL(request.relative_url);
    std::string remaining_path;
    if (absolute_url.path() == "/feeds/default/private/full" &&
        request.method == net::test_server::METHOD_POST) {
      // This is a request for copying a document.
      // TODO(satorux): we should generate valid JSON data for the newly
      // copied document but for now, just return "file_entry.json"
      scoped_ptr<net::test_server::BasicHttpResponse> result(
          test_util::CreateHttpResponseFromFile(
              test_util::GetTestFilePath("gdata/file_entry.json")));
      return result.PassAs<net::test_server::HttpResponse>();
    }

    if (!test_util::RemovePrefix(absolute_url.path(),
                                 "/feeds/default/private/full",
                                 &remaining_path)) {
      return scoped_ptr<net::test_server::HttpResponse>();
    }

    if (remaining_path.empty()) {
      // Process the default feed.
      scoped_ptr<net::test_server::BasicHttpResponse> result(
          test_util::CreateHttpResponseFromFile(
              test_util::GetTestFilePath("gdata/root_feed.json")));
      return result.PassAs<net::test_server::HttpResponse>();
    } else {
      // Process a feed for a single resource ID.
      const std::string resource_id = net::UnescapeURLComponent(
          remaining_path.substr(1), net::UnescapeRule::URL_SPECIAL_CHARS);
      if (resource_id == "file:2_file_resource_id") {
        scoped_ptr<net::test_server::BasicHttpResponse> result(
            test_util::CreateHttpResponseFromFile(
                test_util::GetTestFilePath("gdata/file_entry.json")));
        return result.PassAs<net::test_server::HttpResponse>();
      } else if (resource_id == "folder:root/contents" &&
                 request.method == net::test_server::METHOD_POST) {
        // This is a request for creating a directory in the root directory.
        // TODO(satorux): we should generate valid JSON data for the newly
        // created directory but for now, just return "directory_entry.json"
        scoped_ptr<net::test_server::BasicHttpResponse> result(
            test_util::CreateHttpResponseFromFile(
                test_util::GetTestFilePath(
                    "gdata/directory_entry.json")));
        return result.PassAs<net::test_server::HttpResponse>();
      } else if (resource_id ==
                 "folder:root/contents/file:2_file_resource_id" &&
                 request.method == net::test_server::METHOD_DELETE) {
        // This is a request for deleting a file from the root directory.
        // TODO(satorux): Investigate what's returned from the server, and
        // copy it. For now, just return a random file, as the contents don't
        // matter.
        scoped_ptr<net::test_server::BasicHttpResponse> result(
            test_util::CreateHttpResponseFromFile(
                test_util::GetTestFilePath("gdata/testfile.txt")));
        return result.PassAs<net::test_server::HttpResponse>();
      } else if (resource_id == "invalid_resource_id") {
        // Check if this is an authorization request for an app.
        // This emulates to return invalid formatted result from the server.
        if (request.method == net::test_server::METHOD_PUT &&
            request.content.find("<docs:authorizedApp>") != std::string::npos) {
          scoped_ptr<net::test_server::BasicHttpResponse> result(
              test_util::CreateHttpResponseFromFile(
                  test_util::GetTestFilePath("gdata/testfile.txt")));
          return result.PassAs<net::test_server::HttpResponse>();
        }
      }
    }

    return scoped_ptr<net::test_server::HttpResponse>();
  }

  // Handles a request for fetching a metadata feed.
  scoped_ptr<net::test_server::HttpResponse> HandleMetadataRequest(
      const net::test_server::HttpRequest& request) {
    http_request_ = request;

    const GURL absolute_url = test_server_.GetURL(request.relative_url);
    if (absolute_url.path() != "/feeds/metadata/default")
      return scoped_ptr<net::test_server::HttpResponse>();

    scoped_ptr<net::test_server::BasicHttpResponse> result(
        test_util::CreateHttpResponseFromFile(
            test_util::GetTestFilePath(
                "gdata/account_metadata.json")));
    if (absolute_url.query().find("include-installed-apps=true") ==
        string::npos) {
      // Exclude the list of installed apps.
      scoped_ptr<base::Value> parsed_content(
          base::JSONReader::Read(result->content(), base::JSON_PARSE_RFC));
      CHECK(parsed_content);

      // Remove the install apps node.
      base::DictionaryValue* dictionary_value;
      CHECK(parsed_content->GetAsDictionary(&dictionary_value));
      dictionary_value->Remove("entry.docs$installedApp", NULL);

      // Write back it as the content of the result.
      std::string content;
      base::JSONWriter::Write(parsed_content.get(), &content);
      result->set_content(content);
    }

    return result.PassAs<net::test_server::HttpResponse>();
  }

  // Handles a request for creating a session for uploading.
  scoped_ptr<net::test_server::HttpResponse> HandleCreateSessionRequest(
      const net::test_server::HttpRequest& request) {
    http_request_ = request;

    const GURL absolute_url = test_server_.GetURL(request.relative_url);
    if (StartsWithASCII(absolute_url.path(),
                        "/feeds/upload/create-session/default/private/full",
                        true)) {  // case sensitive
      // This is an initiating upload URL.
      scoped_ptr<net::test_server::BasicHttpResponse> http_response(
          new net::test_server::BasicHttpResponse);

      // Check an ETag.
      std::map<std::string, std::string>::const_iterator found =
          request.headers.find("If-Match");
      if (found != request.headers.end() &&
          found->second != "*" &&
          found->second != kTestETag) {
        http_response->set_code(net::HTTP_PRECONDITION_FAILED);
        return http_response.PassAs<net::test_server::HttpResponse>();
      }

      // Check if the X-Upload-Content-Length is present. If yes, store the
      // length of the file.
      found = request.headers.find("X-Upload-Content-Length");
      if (found == request.headers.end() ||
          !base::StringToInt64(found->second, &content_length_)) {
        return scoped_ptr<net::test_server::HttpResponse>();
      }
      received_bytes_ = 0;

      http_response->set_code(net::HTTP_OK);
      GURL upload_url;
      // POST is used for a new file, and PUT is used for an existing file.
      if (request.method == net::test_server::METHOD_POST) {
        upload_url = test_server_.GetURL("/upload_new_file");
      } else if (request.method == net::test_server::METHOD_PUT) {
        upload_url = test_server_.GetURL("/upload_existing_file");
      } else {
        return scoped_ptr<net::test_server::HttpResponse>();
      }
      http_response->AddCustomHeader("Location", upload_url.spec());
      return http_response.PassAs<net::test_server::HttpResponse>();
    }

    return scoped_ptr<net::test_server::HttpResponse>();
  }

  // Handles a request for uploading content.
  scoped_ptr<net::test_server::HttpResponse> HandleUploadRequest(
      const net::test_server::HttpRequest& request) {
    http_request_ = request;

    const GURL absolute_url = test_server_.GetURL(request.relative_url);
    if (absolute_url.path() != "/upload_new_file" &&
        absolute_url.path() != "/upload_existing_file") {
      return scoped_ptr<net::test_server::HttpResponse>();
    }

    // TODO(satorux): We should create a correct JSON data for the uploaded
    // file, but for now, just return file_entry.json.
    scoped_ptr<net::test_server::BasicHttpResponse> response =
        test_util::CreateHttpResponseFromFile(
            test_util::GetTestFilePath("gdata/file_entry.json"));
    // response.code() is set to SUCCESS. Change it to CREATED if it's a new
    // file.
    if (absolute_url.path() == "/upload_new_file")
      response->set_code(net::HTTP_CREATED);

    // Check if the Content-Range header is present. This must be present if
    // the request body is not empty.
    if (!request.content.empty()) {
      std::map<std::string, std::string>::const_iterator iter =
          request.headers.find("Content-Range");
      if (iter == request.headers.end())
        return scoped_ptr<net::test_server::HttpResponse>();
      int64 length = 0;
      int64 start_position = 0;
      int64 end_position = 0;
      if (!test_util::ParseContentRangeHeader(iter->second,
                                              &start_position,
                                              &end_position,
                                              &length)) {
        return scoped_ptr<net::test_server::HttpResponse>();
      }
      EXPECT_EQ(start_position, received_bytes_);
      EXPECT_EQ(length, content_length_);
      // end_position is inclusive, but so +1 to change the range to byte size.
      received_bytes_ = end_position + 1;
    }

    // Add Range header to the response, based on the values of
    // Content-Range header in the request.
    // The header is annotated only when at least one byte is received.
    if (received_bytes_ > 0) {
      response->AddCustomHeader(
          "Range",
          "bytes=0-" + base::Int64ToString(received_bytes_ - 1));
    }

    // Change the code to RESUME_INCOMPLETE if upload is not complete.
    if (received_bytes_ < content_length_)
      response->set_code(static_cast<net::HttpStatusCode>(308));

    return response.PassAs<net::test_server::HttpResponse>();
  }

  // Handles a request for downloading a file.
  scoped_ptr<net::test_server::HttpResponse> HandleDownloadRequest(
      const net::test_server::HttpRequest& request) {
    http_request_ = request;

    const GURL absolute_url = test_server_.GetURL(request.relative_url);
    std::string id;
    if (!test_util::RemovePrefix(absolute_url.path(),
                                 kTestDownloadPathPrefix,
                                 &id)) {
      return scoped_ptr<net::test_server::HttpResponse>();
    }

    // For testing, returns a text with |id| repeated 3 times.
    scoped_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_OK);
    response->set_content(id + id + id);
    response->set_content_type("text/plain");
    return response.PassAs<net::test_server::HttpResponse>();
  }

  base::MessageLoopForIO message_loop_;  // Test server needs IO thread.
  net::test_server::EmbeddedTestServer test_server_;
  scoped_ptr<RequestSender> request_sender_;
  scoped_ptr<GDataWapiUrlGenerator> url_generator_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  base::ScopedTempDir temp_dir_;

  // These fields are used to keep the current upload state during a
  // test case. These values are updated by the request from
  // ResumeUploadRequest, and used to construct the response for
  // both ResumeUploadRequest and GetUploadStatusRequest, to emulate
  // the WAPI server.
  int64 received_bytes_;
  int64 content_length_;

  // The incoming HTTP request is saved so tests can verify the request
  // parameters like HTTP method (ex. some requests should use DELETE
  // instead of GET).
  net::test_server::HttpRequest http_request_;
};

}  // namespace

TEST_F(GDataWapiRequestsTest, GetResourceListRequest_DefaultFeed) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> result_data;

  {
    base::RunLoop run_loop;
    GetResourceListRequest* request = new GetResourceListRequest(
        request_sender_.get(),
        *url_generator_,
        GURL(),         // Pass an empty URL to use the default feed
        0,              // start changestamp
        std::string(),  // search string
        std::string(),  // directory resource ID
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &result_data)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/feeds/default/private/full?v=3&alt=json&showroot=true&"
            "showfolders=true&include-shared=true&max-results=500",
            http_request_.relative_url);

  // Sanity check of the result.
  scoped_ptr<ResourceList> expected(
      ResourceList::ExtractAndParse(
          *test_util::LoadJSONFile("gdata/root_feed.json")));
  ASSERT_TRUE(result_data);
  EXPECT_EQ(expected->title(), result_data->title());
}

TEST_F(GDataWapiRequestsTest, GetResourceListRequest_ValidFeed) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> result_data;

  {
    base::RunLoop run_loop;
    GetResourceListRequest* request = new GetResourceListRequest(
        request_sender_.get(),
        *url_generator_,
        test_server_.GetURL("/files/gdata/root_feed.json"),
        0,              // start changestamp
        std::string(),  // search string
        std::string(),  // directory resource ID
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &result_data)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/files/gdata/root_feed.json?v=3&alt=json&showroot=true&"
            "showfolders=true&include-shared=true&max-results=500",
            http_request_.relative_url);

  scoped_ptr<ResourceList> expected(
      ResourceList::ExtractAndParse(
          *test_util::LoadJSONFile("gdata/root_feed.json")));
  ASSERT_TRUE(result_data);
  EXPECT_EQ(expected->title(), result_data->title());
}

TEST_F(GDataWapiRequestsTest, GetResourceListRequest_InvalidFeed) {
  // testfile.txt exists but the response is not JSON, so it should
  // emit a parse error instead.
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> result_data;

  {
    base::RunLoop run_loop;
    GetResourceListRequest* request = new GetResourceListRequest(
        request_sender_.get(),
        *url_generator_,
        test_server_.GetURL("/files/gdata/testfile.txt"),
        0,              // start changestamp
        std::string(),  // search string
        std::string(),  // directory resource ID
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &result_data)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(GDATA_PARSE_ERROR, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/files/gdata/testfile.txt?v=3&alt=json&showroot=true&"
            "showfolders=true&include-shared=true&max-results=500",
            http_request_.relative_url);
  EXPECT_FALSE(result_data);
}

TEST_F(GDataWapiRequestsTest, SearchByTitleRequest) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> result_data;

  {
    base::RunLoop run_loop;
    SearchByTitleRequest* request = new SearchByTitleRequest(
        request_sender_.get(),
        *url_generator_,
        "search-title",
        std::string(),  // directory resource id
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &result_data)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/feeds/default/private/full?v=3&alt=json&showroot=true&"
            "showfolders=true&include-shared=true&max-results=500"
            "&title=search-title&title-exact=true",
            http_request_.relative_url);
  EXPECT_TRUE(result_data);
}

TEST_F(GDataWapiRequestsTest, GetResourceEntryRequest_ValidResourceId) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;

  {
    base::RunLoop run_loop;
    GetResourceEntryRequest* request = new GetResourceEntryRequest(
        request_sender_.get(),
        *url_generator_,
        "file:2_file_resource_id",  // resource ID
        GURL(),  // embed origin
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &result_data)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/feeds/default/private/full/file%3A2_file_resource_id"
            "?v=3&alt=json&showroot=true",
            http_request_.relative_url);
  scoped_ptr<base::Value> expected_json =
      test_util::LoadJSONFile("gdata/file_entry.json");
  ASSERT_TRUE(expected_json);
  EXPECT_TRUE(result_data);
  EXPECT_TRUE(base::Value::Equals(expected_json.get(), result_data.get()));
}

TEST_F(GDataWapiRequestsTest, GetResourceEntryRequest_InvalidResourceId) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;

  {
    base::RunLoop run_loop;
    GetResourceEntryRequest* request = new GetResourceEntryRequest(
        request_sender_.get(),
        *url_generator_,
        "<invalid>",  // resource ID
        GURL(),  // embed origin
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &result_data)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_NOT_FOUND, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/feeds/default/private/full/%3Cinvalid%3E?v=3&alt=json"
            "&showroot=true",
            http_request_.relative_url);
  ASSERT_FALSE(result_data);
}

TEST_F(GDataWapiRequestsTest, GetAccountMetadataRequest) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<AccountMetadata> result_data;

  {
    base::RunLoop run_loop;
    GetAccountMetadataRequest* request = new GetAccountMetadataRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &result_data)),
        true);  // Include installed apps.
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/feeds/metadata/default?v=3&alt=json&showroot=true"
            "&include-installed-apps=true",
            http_request_.relative_url);

  scoped_ptr<AccountMetadata> expected(
      AccountMetadata::CreateFrom(
          *test_util::LoadJSONFile("gdata/account_metadata.json")));

  ASSERT_TRUE(result_data.get());
  EXPECT_EQ(expected->largest_changestamp(),
            result_data->largest_changestamp());
  EXPECT_EQ(expected->quota_bytes_total(),
            result_data->quota_bytes_total());
  EXPECT_EQ(expected->quota_bytes_used(),
            result_data->quota_bytes_used());

  // Sanity check for installed apps.
  EXPECT_EQ(expected->installed_apps().size(),
            result_data->installed_apps().size());
}

TEST_F(GDataWapiRequestsTest,
       GetAccountMetadataRequestWithoutInstalledApps) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<AccountMetadata> result_data;

  {
    base::RunLoop run_loop;
    GetAccountMetadataRequest* request = new GetAccountMetadataRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &result_data)),
        false);  // Exclude installed apps.
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/feeds/metadata/default?v=3&alt=json&showroot=true",
            http_request_.relative_url);

  scoped_ptr<AccountMetadata> expected(
      AccountMetadata::CreateFrom(
          *test_util::LoadJSONFile("gdata/account_metadata.json")));

  ASSERT_TRUE(result_data.get());
  EXPECT_EQ(expected->largest_changestamp(),
            result_data->largest_changestamp());
  EXPECT_EQ(expected->quota_bytes_total(),
            result_data->quota_bytes_total());
  EXPECT_EQ(expected->quota_bytes_used(),
            result_data->quota_bytes_used());

  // Installed apps shouldn't be included.
  EXPECT_EQ(0U, result_data->installed_apps().size());
}

TEST_F(GDataWapiRequestsTest, DeleteResourceRequest) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;

  {
    base::RunLoop run_loop;
    DeleteResourceRequest* request = new DeleteResourceRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code)),
        "file:2_file_resource_id",
        std::string());

    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_DELETE, http_request_.method);
  EXPECT_EQ(
      "/feeds/default/private/full/file%3A2_file_resource_id?v=3&alt=json"
      "&showroot=true",
      http_request_.relative_url);
  EXPECT_EQ("*", http_request_.headers["If-Match"]);
}

TEST_F(GDataWapiRequestsTest, DeleteResourceRequestWithETag) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;

  {
    base::RunLoop run_loop;
    DeleteResourceRequest* request = new DeleteResourceRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code)),
        "file:2_file_resource_id",
        "etag");

    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_DELETE, http_request_.method);
  EXPECT_EQ(
      "/feeds/default/private/full/file%3A2_file_resource_id?v=3&alt=json"
      "&showroot=true",
      http_request_.relative_url);
  EXPECT_EQ("etag", http_request_.headers["If-Match"]);
}

TEST_F(GDataWapiRequestsTest, CreateDirectoryRequest) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;

  // Create "new directory" in the root directory.
  {
    base::RunLoop run_loop;
    CreateDirectoryRequest* request = new CreateDirectoryRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &result_data)),
        "folder:root",
        "new directory");

    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/feeds/default/private/full/folder%3Aroot/contents?v=3&alt=json"
            "&showroot=true",
            http_request_.relative_url);
  EXPECT_EQ("application/atom+xml", http_request_.headers["Content-Type"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("<?xml version=\"1.0\"?>\n"
            "<entry xmlns=\"http://www.w3.org/2005/Atom\">\n"
            " <category scheme=\"http://schemas.google.com/g/2005#kind\" "
            "term=\"http://schemas.google.com/docs/2007#folder\"/>\n"
            " <title>new directory</title>\n"
            "</entry>\n",
            http_request_.content);
}

TEST_F(GDataWapiRequestsTest, CopyHostedDocumentRequest) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;

  // Copy a document with a new name "New Document".
  {
    base::RunLoop run_loop;
    CopyHostedDocumentRequest* request = new CopyHostedDocumentRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &result_data)),
        "document:5_document_resource_id",  // source resource ID
        "New Document");

    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/feeds/default/private/full?v=3&alt=json&showroot=true",
            http_request_.relative_url);
  EXPECT_EQ("application/atom+xml", http_request_.headers["Content-Type"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("<?xml version=\"1.0\"?>\n"
            "<entry xmlns=\"http://www.w3.org/2005/Atom\">\n"
            " <id>document:5_document_resource_id</id>\n"
            " <title>New Document</title>\n"
            "</entry>\n",
            http_request_.content);
}

TEST_F(GDataWapiRequestsTest, RenameResourceRequest) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;

  // Rename a file with a new name "New File".
  {
    base::RunLoop run_loop;
    RenameResourceRequest* request = new RenameResourceRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code)),
        "file:2_file_resource_id",
        "New File");

    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ(
      "/feeds/default/private/full/file%3A2_file_resource_id?v=3&alt=json"
      "&showroot=true",
      http_request_.relative_url);
  EXPECT_EQ("application/atom+xml", http_request_.headers["Content-Type"]);
  EXPECT_EQ("*", http_request_.headers["If-Match"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("<?xml version=\"1.0\"?>\n"
            "<entry xmlns=\"http://www.w3.org/2005/Atom\">\n"
            " <title>New File</title>\n"
            "</entry>\n",
            http_request_.content);
}

TEST_F(GDataWapiRequestsTest, AuthorizeAppRequest_ValidFeed) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  GURL result_data;

  // Authorize an app with APP_ID to access to a document.
  {
    base::RunLoop run_loop;
    AuthorizeAppRequest* request = new AuthorizeAppRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &result_data)),
        "file:2_file_resource_id",
        "the_app_id");

    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(GURL("https://entry1_open_with_link/"), result_data);

  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ("/feeds/default/private/full/file%3A2_file_resource_id"
            "?v=3&alt=json&showroot=true",
            http_request_.relative_url);
  EXPECT_EQ("application/atom+xml", http_request_.headers["Content-Type"]);
  EXPECT_EQ("*", http_request_.headers["If-Match"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("<?xml version=\"1.0\"?>\n"
            "<entry xmlns=\"http://www.w3.org/2005/Atom\" "
            "xmlns:docs=\"http://schemas.google.com/docs/2007\">\n"
            " <docs:authorizedApp>the_app_id</docs:authorizedApp>\n"
            "</entry>\n",
            http_request_.content);
}

TEST_F(GDataWapiRequestsTest, AuthorizeAppRequest_NotFound) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  GURL result_data;

  // Authorize an app with APP_ID to access to a document.
  {
    base::RunLoop run_loop;
    AuthorizeAppRequest* request = new AuthorizeAppRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &result_data)),
        "file:2_file_resource_id",
        "unauthorized_app_id");

    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(GDATA_OTHER_ERROR, result_code);
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ("/feeds/default/private/full/file%3A2_file_resource_id"
            "?v=3&alt=json&showroot=true",
            http_request_.relative_url);
  EXPECT_EQ("application/atom+xml", http_request_.headers["Content-Type"]);
  EXPECT_EQ("*", http_request_.headers["If-Match"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("<?xml version=\"1.0\"?>\n"
            "<entry xmlns=\"http://www.w3.org/2005/Atom\" "
            "xmlns:docs=\"http://schemas.google.com/docs/2007\">\n"
            " <docs:authorizedApp>unauthorized_app_id</docs:authorizedApp>\n"
            "</entry>\n",
            http_request_.content);
}

TEST_F(GDataWapiRequestsTest, AuthorizeAppRequest_InvalidFeed) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  GURL result_data;

  // Authorize an app with APP_ID to access to a document but an invalid feed.
  {
    base::RunLoop run_loop;
    AuthorizeAppRequest* request = new AuthorizeAppRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &result_data)),
        "invalid_resource_id",
        "APP_ID");

    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(GDATA_PARSE_ERROR, result_code);
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ("/feeds/default/private/full/invalid_resource_id"
            "?v=3&alt=json&showroot=true",
            http_request_.relative_url);
  EXPECT_EQ("application/atom+xml", http_request_.headers["Content-Type"]);
  EXPECT_EQ("*", http_request_.headers["If-Match"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("<?xml version=\"1.0\"?>\n"
            "<entry xmlns=\"http://www.w3.org/2005/Atom\" "
            "xmlns:docs=\"http://schemas.google.com/docs/2007\">\n"
            " <docs:authorizedApp>APP_ID</docs:authorizedApp>\n"
            "</entry>\n",
            http_request_.content);
}

TEST_F(GDataWapiRequestsTest, AddResourceToDirectoryRequest) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;

  // Add a file to the root directory.
  {
    base::RunLoop run_loop;
    AddResourceToDirectoryRequest* request =
        new AddResourceToDirectoryRequest(
            request_sender_.get(),
            *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&result_code)),
            "folder:root",
            "file:2_file_resource_id");

    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/feeds/default/private/full/folder%3Aroot/contents?v=3&alt=json"
            "&showroot=true",
            http_request_.relative_url);
  EXPECT_EQ("application/atom+xml", http_request_.headers["Content-Type"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(base::StringPrintf("<?xml version=\"1.0\"?>\n"
                               "<entry xmlns=\"http://www.w3.org/2005/Atom\">\n"
                               " <id>%sfeeds/default/private/full/"
                               "file%%3A2_file_resource_id</id>\n"
                               "</entry>\n",
                               test_server_.base_url().spec().c_str()),
            http_request_.content);
}

TEST_F(GDataWapiRequestsTest, RemoveResourceFromDirectoryRequest) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;

  // Remove a file from the root directory.
  {
    base::RunLoop run_loop;
    RemoveResourceFromDirectoryRequest* request =
        new RemoveResourceFromDirectoryRequest(
            request_sender_.get(),
            *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&result_code)),
            "folder:root",
            "file:2_file_resource_id");

    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  // DELETE method should be used, without the body content.
  EXPECT_EQ(net::test_server::METHOD_DELETE, http_request_.method);
  EXPECT_EQ("/feeds/default/private/full/folder%3Aroot/contents/"
            "file%3A2_file_resource_id?v=3&alt=json&showroot=true",
            http_request_.relative_url);
  EXPECT_EQ("*", http_request_.headers["If-Match"]);
  EXPECT_FALSE(http_request_.has_content);
}

// This test exercises InitiateUploadNewFileRequest and
// ResumeUploadRequest for a scenario of uploading a new file.
TEST_F(GDataWapiRequestsTest, UploadNewFile) {
  const std::string kUploadContent = "hello";
  const base::FilePath kTestFilePath =
      temp_dir_.path().AppendASCII("upload_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kUploadContent));

  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  GURL upload_url;

  // 1) Get the upload URL for uploading a new file.
  {
    base::RunLoop run_loop;
    InitiateUploadNewFileRequest* initiate_request =
        new InitiateUploadNewFileRequest(
            request_sender_.get(),
            *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&result_code, &upload_url)),
            "text/plain",
            kUploadContent.size(),
            "folder:id",
            "New file");
    request_sender_->StartRequestWithRetry(initiate_request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server_.GetURL("/upload_new_file"), upload_url);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  // convert=false should be passed as files should be uploaded as-is.
  EXPECT_EQ(
      "/feeds/upload/create-session/default/private/full/folder%3Aid/contents"
      "?convert=false&v=3&alt=json&showroot=true",
      http_request_.relative_url);
  EXPECT_EQ("text/plain", http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ("application/atom+xml", http_request_.headers["Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kUploadContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("<?xml version=\"1.0\"?>\n"
            "<entry xmlns=\"http://www.w3.org/2005/Atom\" "
            "xmlns:docs=\"http://schemas.google.com/docs/2007\">\n"
            " <title>New file</title>\n"
            "</entry>\n",
            http_request_.content);

  // 2) Upload the content to the upload URL.
  UploadRangeResponse response;
  scoped_ptr<ResourceEntry> new_entry;

  {
    base::RunLoop run_loop;
    ResumeUploadRequest* resume_request = new ResumeUploadRequest(
        request_sender_.get(),
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&response, &new_entry)),
        ProgressCallback(),
        upload_url,
        0,  // start_position
        kUploadContent.size(),  // end_position (exclusive)
        kUploadContent.size(),  // content_length,
        "text/plain",  // content_type
        kTestFilePath);

    request_sender_->StartRequestWithRetry(resume_request);
    run_loop.Run();
  }

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should be added.
  EXPECT_EQ("bytes 0-" +
            base::Int64ToString(kUploadContent.size() -1) + "/" +
            base::Int64ToString(kUploadContent.size()),
            http_request_.headers["Content-Range"]);
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kUploadContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_CREATED, response.code);  // Because it's a new file
  // The start and end positions should be set to -1, if an upload is complete.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);
}

// This test exercises InitiateUploadNewFileRequest and ResumeUploadRequest
// for a scenario of uploading a new *large* file, which requires multiple
// requests of ResumeUploadRequest. GetUploadRequest is also tested in this
// test case.
TEST_F(GDataWapiRequestsTest, UploadNewLargeFile) {
  const size_t kMaxNumBytes = 10;
  // This is big enough to cause multiple requests of ResumeUploadRequest
  // as we are going to send at most kMaxNumBytes at a time.
  // So, sending "kMaxNumBytes * 2 + 1" bytes ensures three
  // ResumeUploadRequests, which are start, middle and last requests.
  const std::string kUploadContent(kMaxNumBytes * 2 + 1, 'a');
  const base::FilePath kTestFilePath =
      temp_dir_.path().AppendASCII("upload_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kUploadContent));

  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  GURL upload_url;

  // 1) Get the upload URL for uploading a new file.
  {
    base::RunLoop run_loop;
    InitiateUploadNewFileRequest* initiate_request =
        new InitiateUploadNewFileRequest(
            request_sender_.get(),
            *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&result_code, &upload_url)),
            "text/plain",
            kUploadContent.size(),
            "folder:id",
            "New file");
    request_sender_->StartRequestWithRetry(initiate_request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server_.GetURL("/upload_new_file"), upload_url);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  // convert=false should be passed as files should be uploaded as-is.
  EXPECT_EQ(
      "/feeds/upload/create-session/default/private/full/folder%3Aid/contents"
      "?convert=false&v=3&alt=json&showroot=true",
      http_request_.relative_url);
  EXPECT_EQ("text/plain", http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ("application/atom+xml", http_request_.headers["Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kUploadContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("<?xml version=\"1.0\"?>\n"
            "<entry xmlns=\"http://www.w3.org/2005/Atom\" "
            "xmlns:docs=\"http://schemas.google.com/docs/2007\">\n"
            " <title>New file</title>\n"
            "</entry>\n",
            http_request_.content);

  // 2) Before sending any data, check the current status.
  // This is an edge case test for GetUploadStatusRequest
  // (UploadRangeRequestBase).
  {
    UploadRangeResponse response;
    scoped_ptr<ResourceEntry> new_entry;

    // Check the response by GetUploadStatusRequest.
    {
      base::RunLoop run_loop;
      GetUploadStatusRequest* get_upload_status_request =
          new GetUploadStatusRequest(
              request_sender_.get(),
              test_util::CreateQuitCallback(
                  &run_loop,
                  test_util::CreateCopyResultCallback(&response, &new_entry)),
              upload_url,
              kUploadContent.size());
      request_sender_->StartRequestWithRetry(get_upload_status_request);
      run_loop.Run();
    }

    // METHOD_PUT should be used to upload data.
    EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
    // Request should go to the upload URL.
    EXPECT_EQ(upload_url.path(), http_request_.relative_url);
    // Content-Range header should be added.
    EXPECT_EQ("bytes */" + base::Int64ToString(kUploadContent.size()),
              http_request_.headers["Content-Range"]);
    EXPECT_TRUE(http_request_.has_content);
    EXPECT_TRUE(http_request_.content.empty());

    // Check the response.
    EXPECT_EQ(HTTP_RESUME_INCOMPLETE, response.code);
    EXPECT_EQ(0, response.start_position_received);
    EXPECT_EQ(0, response.end_position_received);
  }

  // 3) Upload the content to the upload URL with multiple requests.
  size_t num_bytes_consumed = 0;
  for (size_t start_position = 0; start_position < kUploadContent.size();
       start_position += kMaxNumBytes) {
    SCOPED_TRACE(testing::Message("start_position: ") << start_position);

    // The payload is at most kMaxNumBytes.
    const size_t remaining_size = kUploadContent.size() - start_position;
    const std::string payload = kUploadContent.substr(
        start_position, std::min(kMaxNumBytes, remaining_size));
    num_bytes_consumed += payload.size();
    // The end position is exclusive.
    const size_t end_position = start_position + payload.size();

    UploadRangeResponse response;
    scoped_ptr<ResourceEntry> new_entry;

    {
      base::RunLoop run_loop;
      ResumeUploadRequest* resume_request = new ResumeUploadRequest(
          request_sender_.get(),
          test_util::CreateQuitCallback(
              &run_loop,
              test_util::CreateCopyResultCallback(&response, &new_entry)),
          ProgressCallback(),
          upload_url,
          start_position,
          end_position,
          kUploadContent.size(),  // content_length,
          "text/plain",  // content_type
          kTestFilePath);
      request_sender_->StartRequestWithRetry(resume_request);
      run_loop.Run();
    }

    // METHOD_PUT should be used to upload data.
    EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
    // Request should go to the upload URL.
    EXPECT_EQ(upload_url.path(), http_request_.relative_url);
    // Content-Range header should be added.
    EXPECT_EQ("bytes " +
              base::Int64ToString(start_position) + "-" +
              base::Int64ToString(end_position - 1) + "/" +
              base::Int64ToString(kUploadContent.size()),
              http_request_.headers["Content-Range"]);
    // The upload content should be set in the HTTP request.
    EXPECT_TRUE(http_request_.has_content);
    EXPECT_EQ(payload, http_request_.content);

    // Check the response.
    if (payload.size() == remaining_size) {
      EXPECT_EQ(HTTP_CREATED, response.code);  // Because it's a new file.
      // The start and end positions should be set to -1, if an upload is
      // complete.
      EXPECT_EQ(-1, response.start_position_received);
      EXPECT_EQ(-1, response.end_position_received);
      // The upload process is completed, so exit from the loop.
      break;
    }

    EXPECT_EQ(HTTP_RESUME_INCOMPLETE, response.code);
    EXPECT_EQ(0, response.start_position_received);
    EXPECT_EQ(static_cast<int64>(end_position),
              response.end_position_received);

    // Check the response by GetUploadStatusRequest.
    {
      base::RunLoop run_loop;
      GetUploadStatusRequest* get_upload_status_request =
          new GetUploadStatusRequest(
              request_sender_.get(),
              test_util::CreateQuitCallback(
                  &run_loop,
                  test_util::CreateCopyResultCallback(&response, &new_entry)),
              upload_url,
              kUploadContent.size());
      request_sender_->StartRequestWithRetry(get_upload_status_request);
      run_loop.Run();
    }

    // METHOD_PUT should be used to upload data.
    EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
    // Request should go to the upload URL.
    EXPECT_EQ(upload_url.path(), http_request_.relative_url);
    // Content-Range header should be added.
    EXPECT_EQ("bytes */" + base::Int64ToString(kUploadContent.size()),
              http_request_.headers["Content-Range"]);
    EXPECT_TRUE(http_request_.has_content);
    EXPECT_TRUE(http_request_.content.empty());

    // Check the response.
    EXPECT_EQ(HTTP_RESUME_INCOMPLETE, response.code);
    EXPECT_EQ(0, response.start_position_received);
    EXPECT_EQ(static_cast<int64>(end_position),
              response.end_position_received);
  }

  EXPECT_EQ(kUploadContent.size(), num_bytes_consumed);
}

// This test exercises InitiateUploadNewFileRequest and ResumeUploadRequest
// for a scenario of uploading a new *empty* file.
//
// The test is almost identical to UploadNewFile. The only difference is the
// expectation for the Content-Range header.
TEST_F(GDataWapiRequestsTest, UploadNewEmptyFile) {
  const std::string kUploadContent;
  const base::FilePath kTestFilePath =
      temp_dir_.path().AppendASCII("empty_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kUploadContent));

  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  GURL upload_url;

  // 1) Get the upload URL for uploading a new file.
  {
    base::RunLoop run_loop;
    InitiateUploadNewFileRequest* initiate_request =
        new InitiateUploadNewFileRequest(
            request_sender_.get(),
            *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&result_code, &upload_url)),
            "text/plain",
            kUploadContent.size(),
            "folder:id",
            "New file");
    request_sender_->StartRequestWithRetry(initiate_request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server_.GetURL("/upload_new_file"), upload_url);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  // convert=false should be passed as files should be uploaded as-is.
  EXPECT_EQ(
      "/feeds/upload/create-session/default/private/full/folder%3Aid/contents"
      "?convert=false&v=3&alt=json&showroot=true",
      http_request_.relative_url);
  EXPECT_EQ("text/plain", http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ("application/atom+xml", http_request_.headers["Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kUploadContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("<?xml version=\"1.0\"?>\n"
            "<entry xmlns=\"http://www.w3.org/2005/Atom\" "
            "xmlns:docs=\"http://schemas.google.com/docs/2007\">\n"
            " <title>New file</title>\n"
            "</entry>\n",
            http_request_.content);

  // 2) Upload the content to the upload URL.
  UploadRangeResponse response;
  scoped_ptr<ResourceEntry> new_entry;

  {
    base::RunLoop run_loop;
    ResumeUploadRequest* resume_request = new ResumeUploadRequest(
        request_sender_.get(),
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&response, &new_entry)),
        ProgressCallback(),
        upload_url,
        0,  // start_position
        kUploadContent.size(),  // end_position (exclusive)
        kUploadContent.size(),  // content_length,
        "text/plain",  // content_type
        kTestFilePath);
    request_sender_->StartRequestWithRetry(resume_request);
    run_loop.Run();
  }

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should not exit if the content is empty.
  // We should not generate the header with an invalid value "bytes 0--1/0".
  EXPECT_EQ(0U, http_request_.headers.count("Content-Range"));
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kUploadContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_CREATED, response.code);  // Because it's a new file.
  // The start and end positions should be set to -1, if an upload is complete.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);
}

// This test exercises InitiateUploadExistingFileRequest and
// ResumeUploadRequest for a scenario of updating an existing file.
TEST_F(GDataWapiRequestsTest, UploadExistingFile) {
  const std::string kUploadContent = "hello";
  const base::FilePath kTestFilePath =
      temp_dir_.path().AppendASCII("upload_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kUploadContent));

  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  GURL upload_url;

  // 1) Get the upload URL for uploading an existing file.
  {
    base::RunLoop run_loop;
    InitiateUploadExistingFileRequest* initiate_request =
        new InitiateUploadExistingFileRequest(
            request_sender_.get(),
            *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&result_code, &upload_url)),
            "text/plain",
            kUploadContent.size(),
            "file:foo",
            std::string() /* etag */);
    request_sender_->StartRequestWithRetry(initiate_request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server_.GetURL("/upload_existing_file"), upload_url);
  // For updating an existing file, METHOD_PUT should be used.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // convert=false should be passed as files should be uploaded as-is.
  EXPECT_EQ("/feeds/upload/create-session/default/private/full/file%3Afoo"
            "?convert=false&v=3&alt=json&showroot=true",
            http_request_.relative_url);
  // Even though the body is empty, the content type should be set to
  // "text/plain".
  EXPECT_EQ("text/plain", http_request_.headers["Content-Type"]);
  EXPECT_EQ("text/plain", http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kUploadContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);
  // For updating an existing file, an empty body should be attached (PUT
  // requires a body)
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("", http_request_.content);
  EXPECT_EQ("*", http_request_.headers["If-Match"]);

  // 2) Upload the content to the upload URL.
  UploadRangeResponse response;
  scoped_ptr<ResourceEntry> new_entry;

  {
    base::RunLoop run_loop;
    ResumeUploadRequest* resume_request = new ResumeUploadRequest(
        request_sender_.get(),
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&response, &new_entry)),
        ProgressCallback(),
        upload_url,
        0,  // start_position
        kUploadContent.size(),  // end_position (exclusive)
        kUploadContent.size(),  // content_length,
        "text/plain",  // content_type
        kTestFilePath);

    request_sender_->StartRequestWithRetry(resume_request);
    run_loop.Run();
  }

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should be added.
  EXPECT_EQ("bytes 0-" +
            base::Int64ToString(kUploadContent.size() -1) + "/" +
            base::Int64ToString(kUploadContent.size()),
            http_request_.headers["Content-Range"]);
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kUploadContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_SUCCESS, response.code);  // Because it's an existing file.
  // The start and end positions should be set to -1, if an upload is complete.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);
}

// This test exercises InitiateUploadExistingFileRequest and
// ResumeUploadRequest for a scenario of updating an existing file.
TEST_F(GDataWapiRequestsTest, UploadExistingFileWithETag) {
  const std::string kUploadContent = "hello";
  const base::FilePath kTestFilePath =
      temp_dir_.path().AppendASCII("upload_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kUploadContent));

  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  GURL upload_url;

  // 1) Get the upload URL for uploading an existing file.
  {
    base::RunLoop run_loop;
    InitiateUploadExistingFileRequest* initiate_request =
        new InitiateUploadExistingFileRequest(
            request_sender_.get(),
            *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&result_code, &upload_url)),
            "text/plain",
            kUploadContent.size(),
            "file:foo",
            kTestETag);
    request_sender_->StartRequestWithRetry(initiate_request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server_.GetURL("/upload_existing_file"), upload_url);
  // For updating an existing file, METHOD_PUT should be used.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // convert=false should be passed as files should be uploaded as-is.
  EXPECT_EQ("/feeds/upload/create-session/default/private/full/file%3Afoo"
            "?convert=false&v=3&alt=json&showroot=true",
            http_request_.relative_url);
  // Even though the body is empty, the content type should be set to
  // "text/plain".
  EXPECT_EQ("text/plain", http_request_.headers["Content-Type"]);
  EXPECT_EQ("text/plain", http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kUploadContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);
  // For updating an existing file, an empty body should be attached (PUT
  // requires a body)
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("", http_request_.content);
  EXPECT_EQ(kTestETag, http_request_.headers["If-Match"]);

  // 2) Upload the content to the upload URL.
  UploadRangeResponse response;
  scoped_ptr<ResourceEntry> new_entry;

  {
    base::RunLoop run_loop;
    ResumeUploadRequest* resume_request = new ResumeUploadRequest(
        request_sender_.get(),
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&response, &new_entry)),
        ProgressCallback(),
        upload_url,
        0,  // start_position
        kUploadContent.size(),  // end_position (exclusive)
        kUploadContent.size(),  // content_length,
        "text/plain",  // content_type
        kTestFilePath);
    request_sender_->StartRequestWithRetry(resume_request);
    run_loop.Run();
  }

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should be added.
  EXPECT_EQ("bytes 0-" +
            base::Int64ToString(kUploadContent.size() -1) + "/" +
            base::Int64ToString(kUploadContent.size()),
            http_request_.headers["Content-Range"]);
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kUploadContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_SUCCESS, response.code);  // Because it's an existing file.
  // The start and end positions should be set to -1, if an upload is complete.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);
}

// This test exercises InitiateUploadExistingFileRequest for a scenario of
// confliction on updating an existing file.
TEST_F(GDataWapiRequestsTest, UploadExistingFileWithETagConflict) {
  const std::string kUploadContent = "hello";
  const std::string kWrongETag = "wrong_etag";
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  GURL upload_url;

  {
    base::RunLoop run_loop;
    InitiateUploadExistingFileRequest* initiate_request =
        new InitiateUploadExistingFileRequest(
            request_sender_.get(),
            *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&result_code, &upload_url)),
            "text/plain",
            kUploadContent.size(),
            "file:foo",
            kWrongETag);
    request_sender_->StartRequestWithRetry(initiate_request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_PRECONDITION, result_code);
  // For updating an existing file, METHOD_PUT should be used.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // convert=false should be passed as files should be uploaded as-is.
  EXPECT_EQ("/feeds/upload/create-session/default/private/full/file%3Afoo"
            "?convert=false&v=3&alt=json&showroot=true",
            http_request_.relative_url);
  // Even though the body is empty, the content type should be set to
  // "text/plain".
  EXPECT_EQ("text/plain", http_request_.headers["Content-Type"]);
  EXPECT_EQ("text/plain", http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kUploadContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);
  // For updating an existing file, an empty body should be attached (PUT
  // requires a body)
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("", http_request_.content);
  EXPECT_EQ(kWrongETag, http_request_.headers["If-Match"]);
}

TEST_F(GDataWapiRequestsTest, DownloadFileRequest) {
  const base::FilePath kDownloadedFilePath =
      temp_dir_.path().AppendASCII("cache_file");
  const std::string kTestIdWithTypeLabel("file:dummyId");
  const std::string kTestId("dummyId");

  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  base::FilePath temp_file;
  {
    base::RunLoop run_loop;
    DownloadFileRequest* request = new DownloadFileRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &temp_file)),
        GetContentCallback(),
        ProgressCallback(),
        kTestIdWithTypeLabel,
        kDownloadedFilePath);
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  std::string contents;
  base::ReadFileToString(temp_file, &contents);
  base::DeleteFile(temp_file, false);

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ(kTestDownloadPathPrefix + kTestId, http_request_.relative_url);
  EXPECT_EQ(kDownloadedFilePath, temp_file);

  const std::string expected_contents = kTestId + kTestId + kTestId;
  EXPECT_EQ(expected_contents, contents);
}

}  // namespace google_apis
