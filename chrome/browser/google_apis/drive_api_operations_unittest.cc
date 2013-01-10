// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_path.h"
#include "base/message_loop_proxy.h"
#include "base/values.h"
#include "chrome/browser/google_apis/drive_api_operations.h"
#include "chrome/browser/google_apis/drive_api_url_generator.h"
#include "chrome/browser/google_apis/operation_registry.h"
#include "chrome/browser/google_apis/test_server/http_server.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/test/test_browser_thread.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kTestDriveApiAuthToken[] = "testtoken";
const char kTestUserAgent[] = "test-user-agent";

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
        base::Bind(&DriveApiOperationsTest::HandleDataFileRequest,
                   base::Unretained(this)));

    url_generator_.reset(new DriveApiUrlGenerator(
        test_util::GetBaseUrlForTesting(test_server_.port())));
  }

  virtual void TearDown() OVERRIDE {
    test_server_.ShutdownAndWaitUntilComplete();
    request_context_getter_ = NULL;
    expected_data_file_path_.clear();
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
  FilePath expected_data_file_path_;

  // The incoming HTTP request is saved so tests can verify the request
  // parameters like HTTP method (ex. some operations should use DELETE
  // instead of GET).
  test_server::HttpRequest http_request_;

 private:
  // Reads the data file of |expected_data_file_path_| and returns its content
  // for the request.
  // To use this method, it is necessary to set |expected_data_file_path_|
  // to the appropriate file path before sending the request to the server.
  scoped_ptr<test_server::HttpResponse> HandleDataFileRequest(
      const test_server::HttpRequest& request) {
    http_request_ = request;

    if (expected_data_file_path_.empty()) {
      // The file is not specified. Delegate the processing to the next
      // handler.
      return scoped_ptr<test_server::HttpResponse>();
    }

    // Return the response from the data file.
    return test_util::CreateHttpResponseFromFile(expected_data_file_path_);
  }
};

TEST_F(DriveApiOperationsTest, GetAboutOperation_ValidFeed) {
  // Set an expected data file containing valid result.
  expected_data_file_path_ = test_util::GetTestFilePath("drive/about.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> feed_data;

  GetAboutOperation* operation = new GetAboutOperation(
      &operation_registry_,
      request_context_getter_.get(),
      *url_generator_,
      base::Bind(&test_util::CopyResultsFromGetDataCallbackAndQuit,
                 &error, &feed_data));
  operation->Start(kTestDriveApiAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/drive/v2/about", http_request_.relative_url);
  EXPECT_TRUE(test_util::VerifyJsonData(
      test_util::GetTestFilePath("drive/about.json"), feed_data.get()));
}

TEST_F(DriveApiOperationsTest, GetAboutOperation_InvalidFeed) {
  // Set an expected data file containing invalid result.
  expected_data_file_path_ = test_util::GetTestFilePath("gdata/testfile.txt");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> feed_data;

  GetAboutOperation* operation = new GetAboutOperation(
      &operation_registry_,
      request_context_getter_.get(),
      *url_generator_,
      base::Bind(&test_util::CopyResultsFromGetDataCallbackAndQuit,
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

}  // namespace google_apis
