// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "chrome/browser/google_apis/gdata_wapi_operations.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"
#include "chrome/browser/google_apis/operation_registry.h"
#include "chrome/browser/google_apis/test_server/http_server.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kTestGDataAuthToken[] = "testtoken";
const char kTestUserAgent[] = "test-user-agent";

// Copies the results from GetDataCallback and quit the message loop.
void CopyResultsFromGetDataCallbackAndQuit(
    GDataErrorCode* out_result_code,
    scoped_ptr<base::Value>* out_result_data,
    GDataErrorCode result_code,
    scoped_ptr<base::Value> result_data) {
  *out_result_code = result_code;
  *out_result_data = result_data.Pass();
  MessageLoop::current()->Quit();
}

// Copies the results from DownloadActionCallback and quit the message loop.
// The contents of the download cache file are copied to a string, and the
// file is removed.
void CopyResultsFromDownloadActionCallbackAndQuit(
    GDataErrorCode* out_result_code,
    std::string* contents,
    GDataErrorCode result_code,
    const FilePath& cache_file_path) {
  *out_result_code = result_code;
  file_util::ReadFileToString(cache_file_path, contents);
  file_util::Delete(cache_file_path, false);
  MessageLoop::current()->Quit();
}

// Copies the result from EntryActionCallback and quit the message loop.
void CopyResultFromEntryActionCallbackAndQuit(
    GDataErrorCode* out_result_code,
    GDataErrorCode result_code) {
  *out_result_code = result_code;
  MessageLoop::current()->Quit();
}

// Returns true if |json_data| equals to JSON data in |expected_json_file_path|.
bool VerifyJsonData(const FilePath& expected_json_file_path,
                    const base::Value* json_data) {
  std::string expected_contents;
  if (!file_util::ReadFileToString(expected_json_file_path, &expected_contents))
    return false;

  scoped_ptr<base::Value> expected_data(
      base::JSONReader::Read(expected_contents));
  return base::Value::Equals(expected_data.get(), json_data);
}

// Returns a HttpResponse created from the given file path.
scoped_ptr<test_server::HttpResponse> CreateHttpResponseFromFile(
    const FilePath& file_path) {
  std::string content;
  if (!file_util::ReadFileToString(file_path, &content))
    return scoped_ptr<test_server::HttpResponse>();

  std::string content_type = "text/plain";
  if (EndsWith(file_path.AsUTF8Unsafe(), ".json", true /* case sensitive */)) {
    content_type = "application/json";
  } else if (EndsWith(file_path.AsUTF8Unsafe(), ".xml", true)) {
    content_type = "text/xml";
  }

  scoped_ptr<test_server::HttpResponse> http_response(
      new test_server::HttpResponse);
  http_response->set_code(test_server::SUCCESS);
  http_response->set_content(content);
  http_response->set_content_type(content_type);
  return http_response.Pass();
}

// Removes |prefix| from |input| and stores the result in |output|. Returns
// true if the prefix is removed.
bool RemovePrefix(const std::string& input,
                  const std::string& prefix,
                  std::string* output) {
  if (!StartsWithASCII(input, prefix, true /* case sensitive */))
    return false;

  *output = input.substr(prefix.size());
  return true;
}

// This class sets a request context getter for testing in
// |testing_browser_process| and then clears the state when an instance of it
// goes out of scope.
class ScopedRequestContextGetterForTesting {
 public:
  ScopedRequestContextGetterForTesting(
      TestingBrowserProcess* testing_browser_process)
      : testing_browser_process_(testing_browser_process) {
    context_getter_ = new net::TestURLRequestContextGetter(
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::IO));
    testing_browser_process_->SetSystemRequestContext(context_getter_.get());
  }

  virtual ~ScopedRequestContextGetterForTesting() {
    testing_browser_process_->SetSystemRequestContext(NULL);
  }

 private:
  scoped_refptr<net::TestURLRequestContextGetter> context_getter_;
  TestingBrowserProcess* testing_browser_process_;
  DISALLOW_COPY_AND_ASSIGN(ScopedRequestContextGetterForTesting);
};

class GDataWapiOperationsTest : public testing::Test {
 public:
  GDataWapiOperationsTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE),
        io_thread_(content::BrowserThread::IO) {
  }

  virtual void SetUp() OVERRIDE {
    file_thread_.Start();
    io_thread_.StartIOThread();
    profile_.reset(new TestingProfile);

    // Set a context getter in |g_browser_process|. This is required to be able
    // to use net::URLFetcher.
    request_context_getter_.reset(
        new ScopedRequestContextGetterForTesting(
            static_cast<TestingBrowserProcess*>(g_browser_process)));

    ASSERT_TRUE(test_server_.InitializeAndWaitUntilReady());
    test_server_.RegisterRequestHandler(
        base::Bind(&GDataWapiOperationsTest::HandleDownloadRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&GDataWapiOperationsTest::HandleResourceFeedRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&GDataWapiOperationsTest::HandleMetadataFeedRequest,
                   base::Unretained(this)));

    url_generator_.reset(new GDataWapiUrlGenerator(
        GDataWapiUrlGenerator::GetBaseUrlForTesting(test_server_.port())));
  }

  virtual void TearDown() OVERRIDE {
    test_server_.ShutdownAndWaitUntilComplete();
    request_context_getter_.reset();
  }

 protected:
  // Returns a temporary file path suitable for storing the cache file.
  FilePath GetTestCachedFilePath(const FilePath& file_name) {
    return profile_->GetPath().Append(file_name);
  }

  // Handles a request for downloading a file. Reads a file from the test
  // directory and returns the content.
  scoped_ptr<test_server::HttpResponse> HandleDownloadRequest(
      const test_server::HttpRequest& request) {
    http_request_ = request;

    const GURL absolute_url = test_server_.GetURL(request.relative_url);
    std::string remaining_path;
    if (!RemovePrefix(absolute_url.path(), "/files/", &remaining_path))
      return scoped_ptr<test_server::HttpResponse>();

    return CreateHttpResponseFromFile(
        test_util::GetTestFilePath(remaining_path));
  }

  // Handles a request for fetching a resource feed.
  scoped_ptr<test_server::HttpResponse> HandleResourceFeedRequest(
      const test_server::HttpRequest& request) {
    http_request_ = request;

    const GURL absolute_url = test_server_.GetURL(request.relative_url);
    std::string remaining_path;
    if (absolute_url.path() == "/feeds/default/private/full" &&
        request.method == test_server::METHOD_POST) {
      // This is a request for copying a document.
      // TODO(satorux): we should generate valid JSON data for the newly
      // copied document but for now, just return "file_entry.json"
      return CreateHttpResponseFromFile(
          test_util::GetTestFilePath("gdata/file_entry.json"));
    }

    if (!RemovePrefix(absolute_url.path(),
                      "/feeds/default/private/full/",
                      &remaining_path)) {
      return scoped_ptr<test_server::HttpResponse>();
    }

    if (remaining_path == "-/mine") {
      // Process the default feed.
      return CreateHttpResponseFromFile(
          test_util::GetTestFilePath("gdata/root_feed.json"));
    } else {
      // Process a feed for a single resource ID.
      const std::string resource_id = net::UnescapeURLComponent(
          remaining_path, net::UnescapeRule::URL_SPECIAL_CHARS);
      if (resource_id == "file:2_file_resource_id") {
        // Check if this is an authorization request for an app.
        if (request.method == test_server::METHOD_PUT &&
            request.content.find("<docs:authorizedApp>") != std::string::npos) {
          return CreateHttpResponseFromFile(
              test_util::GetTestFilePath("gdata/entry.xml"));
        }

        return CreateHttpResponseFromFile(
            test_util::GetTestFilePath("gdata/file_entry.json"));
      } else if (resource_id == "folder:root" &&
                 request.method == test_server::METHOD_POST) {
        // This is a request for creating a directory in the root directory.
        // TODO(satorux): we should generate valid JSON data for the newly
        // created directory but for now, just return "directory_entry.json"
        return CreateHttpResponseFromFile(
            test_util::GetTestFilePath("gdata/directory_entry.json"));
      }
    }

    return scoped_ptr<test_server::HttpResponse>();
  }

  // Handles a request for fetching a metadata feed.
  scoped_ptr<test_server::HttpResponse> HandleMetadataFeedRequest(
      const test_server::HttpRequest& request) {
    http_request_ = request;

    const GURL absolute_url = test_server_.GetURL(request.relative_url);
    if (absolute_url.path() != "/feeds/metadata/default")
      return scoped_ptr<test_server::HttpResponse>();

    return CreateHttpResponseFromFile(
        test_util::GetTestFilePath("gdata/account_metadata.json"));
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;
  test_server::HttpServer test_server_;
  scoped_ptr<TestingProfile> profile_;
  OperationRegistry operation_registry_;
  scoped_ptr<GDataWapiUrlGenerator> url_generator_;
  scoped_ptr<ScopedRequestContextGetterForTesting> request_context_getter_;

  // The incoming HTTP request is saved so tests can verify the request
  // parameters like HTTP method (ex. some operations should use DELETE
  // instead of GET).
  test_server::HttpRequest http_request_;
};

}  // namespace

TEST_F(GDataWapiOperationsTest, GetDocumentsOperation_DefaultFeed) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;

  GetDocumentsOperation* operation = new GetDocumentsOperation(
      &operation_registry_,
      *url_generator_,
      GURL(),  // Pass an empty URL to use the default feed
      0,  // start changestamp
      "",  // search string
      false,  // shared with me
      "",  // directory resource ID
      base::Bind(&CopyResultsFromGetDataCallbackAndQuit,
                 &result_code,
                 &result_data));
  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server::METHOD_GET, http_request_.method);
  ASSERT_TRUE(result_data);
  EXPECT_TRUE(VerifyJsonData(
      test_util::GetTestFilePath("gdata/root_feed.json"),
      result_data.get()));
}

TEST_F(GDataWapiOperationsTest, GetDocumentsOperation_ValidFeed) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;

  GetDocumentsOperation* operation = new GetDocumentsOperation(
      &operation_registry_,
      *url_generator_,
      test_server_.GetURL("/files/gdata/root_feed.json"),
      0,  // start changestamp
      "",  // search string
      false,  // shared with me
      "",  // directory resource ID
      base::Bind(&CopyResultsFromGetDataCallbackAndQuit,
                 &result_code,
                 &result_data));
  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server::METHOD_GET, http_request_.method);
  ASSERT_TRUE(result_data);
  EXPECT_TRUE(VerifyJsonData(
      test_util::GetTestFilePath("gdata/root_feed.json"),
      result_data.get()));
}

TEST_F(GDataWapiOperationsTest, GetDocumentsOperation_InvalidFeed) {
  // testfile.txt exists but the response is not JSON, so it should
  // emit a parse error instead.
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;

  GetDocumentsOperation* operation = new GetDocumentsOperation(
      &operation_registry_,
      *url_generator_,
      test_server_.GetURL("/files/gdata/testfile.txt"),
      0,  // start changestamp
      "",  // search string
      false,  // shared with me
      "",  // directory resource ID
      base::Bind(&CopyResultsFromGetDataCallbackAndQuit,
                 &result_code,
                 &result_data));
  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(GDATA_PARSE_ERROR, result_code);
  EXPECT_EQ(test_server::METHOD_GET, http_request_.method);
  EXPECT_FALSE(result_data);
}

TEST_F(GDataWapiOperationsTest, GetDocumentEntryOperation_ValidResourceId) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;

  GetDocumentEntryOperation* operation = new GetDocumentEntryOperation(
          &operation_registry_,
          *url_generator_,
          "file:2_file_resource_id",  // resource ID
          base::Bind(&CopyResultsFromGetDataCallbackAndQuit,
                     &result_code,
                     &result_data));
  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server::METHOD_GET, http_request_.method);
  ASSERT_TRUE(result_data);
  EXPECT_TRUE(VerifyJsonData(
      test_util::GetTestFilePath("gdata/file_entry.json"),
      result_data.get()));
}

TEST_F(GDataWapiOperationsTest, GetDocumentEntryOperation_InvalidResourceId) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;

  GetDocumentEntryOperation* operation = new GetDocumentEntryOperation(
          &operation_registry_,
          *url_generator_,
          "<invalid>",  // resource ID
          base::Bind(&CopyResultsFromGetDataCallbackAndQuit,
                     &result_code,
                     &result_data));
  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_NOT_FOUND, result_code);
  EXPECT_EQ(test_server::METHOD_GET, http_request_.method);
  ASSERT_FALSE(result_data);
}

TEST_F(GDataWapiOperationsTest, GetAccountMetadataOperation) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;

  GetAccountMetadataOperation* operation =
      new google_apis::GetAccountMetadataOperation(
          &operation_registry_,
          *url_generator_,
          base::Bind(&CopyResultsFromGetDataCallbackAndQuit,
                     &result_code,
                     &result_data));
  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server::METHOD_GET, http_request_.method);
  EXPECT_TRUE(VerifyJsonData(
      test_util::GetTestFilePath("gdata/account_metadata.json"),
      result_data.get()));
}

TEST_F(GDataWapiOperationsTest, DownloadFileOperation_ValidFile) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  std::string contents;
  DownloadFileOperation* operation = new DownloadFileOperation(
      &operation_registry_,
      base::Bind(&CopyResultsFromDownloadActionCallbackAndQuit,
                 &result_code,
                 &contents),
      GetContentCallback(),
      test_server_.GetURL("/files/gdata/testfile.txt"),
      FilePath::FromUTF8Unsafe("/dummy/gdata/testfile.txt"),
      GetTestCachedFilePath(FilePath::FromUTF8Unsafe("cached_testfile.txt")));
  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server::METHOD_GET, http_request_.method);

  const FilePath expected_path =
      test_util::GetTestFilePath("gdata/testfile.txt");
  std::string expected_contents;
  file_util::ReadFileToString(expected_path, &expected_contents);
  EXPECT_EQ(expected_contents, contents);
}

TEST_F(GDataWapiOperationsTest, DownloadFileOperation_NonExistentFile) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  std::string contents;
  DownloadFileOperation* operation = new DownloadFileOperation(
      &operation_registry_,
      base::Bind(&CopyResultsFromDownloadActionCallbackAndQuit,
                 &result_code,
                 &contents),
      GetContentCallback(),
      test_server_.GetURL("/files/gdata/no-such-file.txt"),
      FilePath::FromUTF8Unsafe("/dummy/gdata/no-such-file.txt"),
      GetTestCachedFilePath(
          FilePath::FromUTF8Unsafe("cache_no-such-file.txt")));
  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_NOT_FOUND, result_code);
  EXPECT_EQ(test_server::METHOD_GET, http_request_.method);
  // Do not verify the not found message.
}

TEST_F(GDataWapiOperationsTest, DeleteDocumentOperation) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;

  DeleteDocumentOperation* operation = new DeleteDocumentOperation(
      &operation_registry_,
      base::Bind(&CopyResultFromEntryActionCallbackAndQuit,
                 &result_code),
      test_server_.GetURL(
          "/feeds/default/private/full/file:2_file_resource_id"));

  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server::METHOD_DELETE, http_request_.method);
  EXPECT_EQ("*", http_request_.headers["If-Match"]);
}

TEST_F(GDataWapiOperationsTest, CreateDirectoryOperation) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;

  // Create "new directory" in the root directory.
  CreateDirectoryOperation* operation = new CreateDirectoryOperation(
      &operation_registry_,
      *url_generator_,
      base::Bind(&CopyResultsFromGetDataCallbackAndQuit,
                 &result_code,
                 &result_data),
      test_server_.GetURL("/feeds/default/private/full/folder%3Aroot"),
      FILE_PATH_LITERAL("new directory"));

  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server::METHOD_POST, http_request_.method);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("<?xml version=\"1.0\"?>\n"
            "<entry xmlns=\"http://www.w3.org/2005/Atom\">\n"
            " <category scheme=\"http://schemas.google.com/g/2005#kind\" "
            "term=\"http://schemas.google.com/docs/2007#folder\"/>\n"
            " <title>new directory</title>\n"
            "</entry>\n",
            http_request_.content);
}

TEST_F(GDataWapiOperationsTest, CopyDocumentOperation) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;

  // Copy a document with a new name "New Document".
  CopyDocumentOperation* operation = new CopyDocumentOperation(
      &operation_registry_,
      *url_generator_,
      base::Bind(&CopyResultsFromGetDataCallbackAndQuit,
                 &result_code,
                 &result_data),
      "document:5_document_resource_id",  // source resource ID
      FILE_PATH_LITERAL("New Document"));

  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server::METHOD_POST, http_request_.method);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("<?xml version=\"1.0\"?>\n"
            "<entry xmlns=\"http://www.w3.org/2005/Atom\">\n"
            " <id>document:5_document_resource_id</id>\n"
            " <title>New Document</title>\n"
            "</entry>\n",
            http_request_.content);
}

TEST_F(GDataWapiOperationsTest, RenameResourceOperation) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;

  // Rename a file with a new name "New File".
  RenameResourceOperation* operation = new RenameResourceOperation(
      &operation_registry_,
      base::Bind(&CopyResultFromEntryActionCallbackAndQuit,
                 &result_code),
      test_server_.GetURL(
          "/feeds/default/private/full/file:2_file_resource_id"),
      FILE_PATH_LITERAL("New File"));

  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server::METHOD_PUT, http_request_.method);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("<?xml version=\"1.0\"?>\n"
            "<entry xmlns=\"http://www.w3.org/2005/Atom\">\n"
            " <title>New File</title>\n"
            "</entry>\n",
            http_request_.content);
}

TEST_F(GDataWapiOperationsTest, AuthorizeAppOperation_ValidFeed) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;

  // Authorize an app with APP_ID to access to a document.
  AuthorizeAppOperation* operation = new AuthorizeAppOperation(
      &operation_registry_,
      base::Bind(&CopyResultsFromGetDataCallbackAndQuit,
                 &result_code,
                 &result_data),
      test_server_.GetURL(
          "/feeds/default/private/full/file:2_file_resource_id"),
      "APP_ID");

  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ("*", http_request_.headers["If-Match"]);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("<?xml version=\"1.0\"?>\n"
            "<entry xmlns=\"http://www.w3.org/2005/Atom\" "
            "xmlns:docs=\"http://schemas.google.com/docs/2007\">\n"
            " <docs:authorizedApp>APP_ID</docs:authorizedApp>\n"
            "</entry>\n",
            http_request_.content);
}

TEST_F(GDataWapiOperationsTest, AuthorizeAppOperation_InvalidFeed) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;

  // Authorize an app with APP_ID to access to a document but an invalid feed.
  AuthorizeAppOperation* operation = new AuthorizeAppOperation(
      &operation_registry_,
      base::Bind(&CopyResultsFromGetDataCallbackAndQuit,
                 &result_code,
                 &result_data),
      test_server_.GetURL("/files/gdata/testfile.txt"),
      "APP_ID");

  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(GDATA_PARSE_ERROR, result_code);
  EXPECT_EQ(test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ("*", http_request_.headers["If-Match"]);
  EXPECT_EQ("<?xml version=\"1.0\"?>\n"
            "<entry xmlns=\"http://www.w3.org/2005/Atom\" "
            "xmlns:docs=\"http://schemas.google.com/docs/2007\">\n"
            " <docs:authorizedApp>APP_ID</docs:authorizedApp>\n"
            "</entry>\n",
            http_request_.content);
}

TEST_F(GDataWapiOperationsTest, AddResourceToDirectoryOperation) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;

  // Add a file to the root directory.
  AddResourceToDirectoryOperation* operation =
      new AddResourceToDirectoryOperation(
          &operation_registry_,
          *url_generator_,
          base::Bind(&CopyResultFromEntryActionCallbackAndQuit,
                     &result_code),
          test_server_.GetURL("/feeds/default/private/full/folder%3Aroot"),
          test_server_.GetURL(
              "/feeds/default/private/full/file:2_file_resource_id"));

  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server::METHOD_POST, http_request_.method);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("<?xml version=\"1.0\"?>\n"
            "<entry xmlns=\"http://www.w3.org/2005/Atom\">\n"
            " <id>http://127.0.0.1:8040/feeds/default/private/full/"
            "file:2_file_resource_id</id>\n"
            "</entry>\n",
            http_request_.content);
}

// TODO(satorux): Write tests for RemoveResourceFromDirectoryOperation.
// crbug.com/162348

// TODO(satorux): Write tests for InitiateUploadOperation.
// crbug.com/162348

// TODO(satorux): Write tests for ResumeUploadOperation.
// crbug.com/162348

}  // namespace google_apis
