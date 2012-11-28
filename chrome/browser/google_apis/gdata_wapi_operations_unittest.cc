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
void CopyResultsFromDownloadActionCallback(
    GDataErrorCode* out_result_code,
    std::string* contents,
    GDataErrorCode result_code,
    const FilePath& cache_file_path) {
  *out_result_code = result_code;
  file_util::ReadFileToString(cache_file_path, contents);
  file_util::Delete(cache_file_path, false);
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

// Returns a HttpResponse created from the given JSON file path.
scoped_ptr<test_server::HttpResponse> CreateJsonResponseFromFile(
    const FilePath& file_path) {
  std::string content;
  if (!file_util::ReadFileToString(file_path, &content))
    return scoped_ptr<test_server::HttpResponse>();

  scoped_ptr<test_server::HttpResponse> http_response(
      new test_server::HttpResponse);
  http_response->set_code(test_server::SUCCESS);
  http_response->set_content(content);
  http_response->set_content_type("application/json");
  return http_response.Pass();
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
    test_server_.RegisterFileResponse(
        "/files/chromeos/gdata/testfile.txt",
        test_util::GetTestFilePath("gdata/testfile.txt"),
        "text/plain",
        test_server::SUCCESS);
    test_server_.RegisterFileResponse(
        "/files/chromeos/gdata/root_feed.json",
        test_util::GetTestFilePath("gdata/root_feed.json"),
        "text/plain",
        test_server::SUCCESS);
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

  // Handles a request for fetching a resource feed.
  scoped_ptr<test_server::HttpResponse> HandleResourceFeedRequest(
      const test_server::HttpRequest& request) {
    const char kFeedPrefix[] = "/feeds/default/private/full/";
    if (!StartsWithASCII(request.relative_url,
                         kFeedPrefix,
                         true /* case sensitive */)) {
      return scoped_ptr<test_server::HttpResponse>();
    }

    const GURL absolute_url = test_server_.GetURL(request.relative_url);
    // Remove the feed prefix.
    std::string suffix = absolute_url.path();
    ReplaceFirstSubstringAfterOffset(&suffix, 0, kFeedPrefix, "");

    if (suffix == "-/mine") {
      // Process the default feed.
      return CreateJsonResponseFromFile(
          test_util::GetTestFilePath("gdata/root_feed.json"));
    } else {
      // Process a feed for a single resource ID.
      // For now, we only support a resource feed for a particular entry.
      const std::string resource_id = net::UnescapeURLComponent(
          suffix, net::UnescapeRule::URL_SPECIAL_CHARS);
      if (resource_id != "file:2_file_resource_id")
        return scoped_ptr<test_server::HttpResponse>();

      return CreateJsonResponseFromFile(
          test_util::GetTestFilePath("gdata/file_entry.json"));
    }

    NOTREACHED();
    return scoped_ptr<test_server::HttpResponse>();
  }

  // Handles a request for fetching a metadata feed.
  scoped_ptr<test_server::HttpResponse> HandleMetadataFeedRequest(
      const test_server::HttpRequest& request) {
    const GURL absolute_url = test_server_.GetURL(request.relative_url);
    if (absolute_url.path() != "/feeds/metadata/default")
      return scoped_ptr<test_server::HttpResponse>();

    return CreateJsonResponseFromFile(
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
      test_server_.GetURL("/files/chromeos/gdata/root_feed.json"),
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
      test_server_.GetURL("/files/chromeos/gdata/testfile.txt"),
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
  EXPECT_TRUE(VerifyJsonData(
      test_util::GetTestFilePath("gdata/account_metadata.json"),
      result_data.get()));
}

TEST_F(GDataWapiOperationsTest, DownloadFileOperation_ValidFile) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  std::string contents;
  DownloadFileOperation* operation = new DownloadFileOperation(
      &operation_registry_,
      base::Bind(&CopyResultsFromDownloadActionCallback,
                 &result_code,
                 &contents),
      GetContentCallback(),
      test_server_.GetURL("/files/chromeos/gdata/testfile.txt"),
      FilePath::FromUTF8Unsafe("/dummy/gdata/testfile.txt"),
      GetTestCachedFilePath(FilePath::FromUTF8Unsafe("cached_testfile.txt")));
  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, result_code);
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
      base::Bind(&CopyResultsFromDownloadActionCallback,
                 &result_code,
                 &contents),
      GetContentCallback(),
      test_server_.GetURL("/files/chromeos/gdata/no-such-file.txt"),
      FilePath::FromUTF8Unsafe("/dummy/gdata/no-such-file.txt"),
      GetTestCachedFilePath(
          FilePath::FromUTF8Unsafe("cache_no-such-file.txt")));
  operation->Start(kTestGDataAuthToken, kTestUserAgent);
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_NOT_FOUND, result_code);
  // Do not verify the not found message.
}

// TODO(satorux): Write tests for DeleteDocumentOperation.
// crbug.com/162348

// TODO(satorux): Write tests for CreateDirectoryOperation.
// crbug.com/162348

// TODO(satorux): Write tests for CopyDocumentOperation.
// crbug.com/162348

// TODO(satorux): Write tests for RenameResourceOperation.
// crbug.com/162348

// TODO(satorux): Write tests for AuthorizeAppsOperation.
// crbug.com/162348

// TODO(satorux): Write tests for AddResourceToDirectoryOperation.
// crbug.com/162348

// TODO(satorux): Write tests for RemoveResourceFromDirectoryOperation.
// crbug.com/162348

// TODO(satorux): Write tests for InitiateUploadOperation.
// crbug.com/162348

// TODO(satorux): Write tests for ResumeUploadOperation.
// crbug.com/162348

}  // namespace google_apis
