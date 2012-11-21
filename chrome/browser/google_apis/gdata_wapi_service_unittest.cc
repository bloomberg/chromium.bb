// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "chrome/browser/google_apis/auth_service.h"
#include "chrome/browser/google_apis/gdata_wapi_service.h"
#include "chrome/browser/google_apis/test_server/http_server.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kTestGDataAuthToken[] = "testtoken";

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

class GDataWapiServiceTest : public testing::Test {
 public:
  GDataWapiServiceTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE),
        io_thread_(content::BrowserThread::IO) {
  }

  virtual void SetUp() OVERRIDE {
    file_thread_.Start();
    io_thread_.StartIOThread();
    profile_.reset(new TestingProfile);

    service_.reset(new GDataWapiService(
        GURL(GDataWapiUrlGenerator::kBaseUrlForProduction),
        "" /* custom_user_agent*/));
    service_->Initialize(profile_.get());
    service_->auth_service_for_testing()->set_access_token_for_testing(
        kTestGDataAuthToken);

    // Set a context getter in |g_browser_process|. This is required to be able
    // to use net::URLFetcher.
    request_context_getter_.reset(
        new ScopedRequestContextGetterForTesting(
            static_cast<TestingBrowserProcess*>(g_browser_process)));

    ASSERT_TRUE(gdata_test_server_.InitializeAndWaitUntilReady());
    gdata_test_server_.RegisterFileResponse(
        "/files/chromeos/gdata/testfile.txt",
        test_util::GetTestFilePath("gdata/testfile.txt"),
        "text/plain",
        test_server::SUCCESS);
    gdata_test_server_.RegisterFileResponse(
        "/files/chromeos/gdata/root_feed.json",
        test_util::GetTestFilePath("gdata/root_feed.json"),
        "text/plain",
        test_server::SUCCESS);
  }

  virtual void TearDown() OVERRIDE {
    gdata_test_server_.ShutdownAndWaitUntilComplete();
    service_.reset();
    request_context_getter_.reset();
  }

 protected:
  FilePath GetTestCachedFilePath(const FilePath& file_name) {
    return profile_->GetPath().Append(file_name);
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;
  test_server::HttpServer gdata_test_server_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<GDataWapiService> service_;
  scoped_ptr<ScopedRequestContextGetterForTesting> request_context_getter_;
};

// The test callback for GDataWapiService::DownloadFile().
void TestDownloadCallback(GDataErrorCode* result,
                          std::string* contents,
                          GDataErrorCode error,
                          const GURL& content_url,
                          const FilePath& temp_file) {
  *result = error;
  file_util::ReadFileToString(temp_file, contents);
  file_util::Delete(temp_file, false);
  MessageLoop::current()->Quit();
}

// The test callback for GDataWapiService::GetDocuments().
void TestGetDocumentsCallback(GDataErrorCode* result_code,
                              scoped_ptr<base::Value>* result_data,
                              GDataErrorCode error,
                              scoped_ptr<base::Value> feed_data) {
  *result_code = error;
  *result_data = feed_data.Pass();
  MessageLoop::current()->Quit();
}

}  // namespace

TEST_F(GDataWapiServiceTest, Download) {
  GDataErrorCode result = GDATA_OTHER_ERROR;
  std::string contents;
  service_->DownloadFile(
      FilePath(FILE_PATH_LITERAL("/dummy/gdata/testfile.txt")),
      GetTestCachedFilePath(FilePath(
          FILE_PATH_LITERAL("cached_testfile.txt"))),
      gdata_test_server_.GetURL("/files/chromeos/gdata/testfile.txt"),
      base::Bind(&TestDownloadCallback, &result, &contents),
      GetContentCallback());
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, result);
  const FilePath expected_path =
      test_util::GetTestFilePath("gdata/testfile.txt");
  std::string expected_contents;
  file_util::ReadFileToString(expected_path, &expected_contents);
  EXPECT_EQ(expected_contents, contents);
}

TEST_F(GDataWapiServiceTest, NonExistingDownload) {
  GDataErrorCode result = GDATA_OTHER_ERROR;
  std::string dummy_contents;
  service_->DownloadFile(
      FilePath(FILE_PATH_LITERAL("/dummy/gdata/no-such-file.txt")),
      GetTestCachedFilePath(FilePath(
          FILE_PATH_LITERAL("cache_no-such-file.txt"))),
      gdata_test_server_.GetURL("/files/chromeos/gdata/no-such-file.txt"),
      base::Bind(&TestDownloadCallback, &result, &dummy_contents),
      GetContentCallback());
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_NOT_FOUND, result);
  // Do not verify the not found message.
}

TEST_F(GDataWapiServiceTest, GetDocuments) {
  GDataErrorCode result = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;
  service_->GetDocuments(
      gdata_test_server_.GetURL("/files/chromeos/gdata/root_feed.json"),
      0,  // start_changestamp
      std::string(),  // search string
      false,  // shared with me
      std::string(),  // directory resource ID
      base::Bind(&TestGetDocumentsCallback, &result, &result_data));
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, result);
  ASSERT_TRUE(result_data);
  const FilePath expected_path =
      test_util::GetTestFilePath("gdata/root_feed.json");
  std::string expected_contents;
  file_util::ReadFileToString(expected_path, &expected_contents);
  scoped_ptr<base::Value> expected_data(
      base::JSONReader::Read(expected_contents));
  EXPECT_TRUE(base::Value::Equals(expected_data.get(), result_data.get()));
}

TEST_F(GDataWapiServiceTest, GetDocumentsFailure) {
  // testfile.txt exists but the response is not JSON, so it should
  // emit a parse error instead.
  GDataErrorCode result = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;
  service_->GetDocuments(
      gdata_test_server_.GetURL("/files/chromeos/gdata/testfile.txt"),
      0,  // start_changestamp
      std::string(),  // search string
      false,  // shared with me
      std::string(),  // directory resource ID
      base::Bind(&TestGetDocumentsCallback, &result, &result_data));
  MessageLoop::current()->Run();

  EXPECT_EQ(GDATA_PARSE_ERROR, result);
  EXPECT_FALSE(result_data);
}

}  // namespace google_apis
