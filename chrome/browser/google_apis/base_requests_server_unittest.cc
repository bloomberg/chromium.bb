// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/base_requests.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/google_apis/auth_service.h"
#include "chrome/browser/google_apis/request_sender.h"
#include "chrome/browser/google_apis/task_util.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kTestAuthToken[] = "testtoken";
const char kTestUserAgent[] = "test-user-agent";

}  // namespace

class BaseRequestsServerTest : public testing::Test {
 protected:
  BaseRequestsServerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD),
        test_server_(content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::IO)) {
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);

    request_context_getter_ = new net::TestURLRequestContextGetter(
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::IO));

    request_sender_.reset(new RequestSender(profile_.get(),
                                            request_context_getter_.get(),
                                            std::vector<std::string>(),
                                            kTestUserAgent));
    request_sender_->auth_service()->set_access_token_for_testing(
        kTestAuthToken);

    ASSERT_TRUE(test_server_.InitializeAndWaitUntilReady());
    test_server_.RegisterRequestHandler(
        base::Bind(&test_util::HandleDownloadFileRequest,
                   test_server_.base_url(),
                   base::Unretained(&http_request_)));
  }

  virtual void TearDown() OVERRIDE {
    EXPECT_TRUE(test_server_.ShutdownAndWaitUntilComplete());
    request_context_getter_ = NULL;
  }

  // Returns a temporary file path suitable for storing the cache file.
  base::FilePath GetTestCachedFilePath(const base::FilePath& file_name) {
    return profile_->GetPath().Append(file_name);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  net::test_server::EmbeddedTestServer test_server_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<RequestSender> request_sender_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;

  // The incoming HTTP request is saved so tests can verify the request
  // parameters like HTTP method (ex. some requests should use DELETE
  // instead of GET).
  net::test_server::HttpRequest http_request_;
};

TEST_F(BaseRequestsServerTest, DownloadFileRequest_ValidFile) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  base::FilePath temp_file;
  {
    base::RunLoop run_loop;
    DownloadFileRequest* request = new DownloadFileRequest(
        request_sender_.get(),
        request_context_getter_.get(),
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &temp_file)),
        GetContentCallback(),
        ProgressCallback(),
        test_server_.GetURL("/files/chromeos/gdata/testfile.txt"),
        GetTestCachedFilePath(
            base::FilePath::FromUTF8Unsafe("cached_testfile.txt")));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  std::string contents;
  file_util::ReadFileToString(temp_file, &contents);
  file_util::Delete(temp_file, false);

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/files/chromeos/gdata/testfile.txt", http_request_.relative_url);

  const base::FilePath expected_path =
      test_util::GetTestFilePath("chromeos/gdata/testfile.txt");
  std::string expected_contents;
  file_util::ReadFileToString(expected_path, &expected_contents);
  EXPECT_EQ(expected_contents, contents);
}

TEST_F(BaseRequestsServerTest, DownloadFileRequest_NonExistentFile) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  base::FilePath temp_file;
  {
    base::RunLoop run_loop;
    DownloadFileRequest* request = new DownloadFileRequest(
        request_sender_.get(),
        request_context_getter_.get(),
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &temp_file)),
        GetContentCallback(),
        ProgressCallback(),
        test_server_.GetURL("/files/chromeos/gdata/no-such-file.txt"),
        GetTestCachedFilePath(
            base::FilePath::FromUTF8Unsafe("cache_no-such-file.txt")));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }
  EXPECT_EQ(HTTP_NOT_FOUND, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/files/chromeos/gdata/no-such-file.txt",
            http_request_.relative_url);
  // Do not verify the not found message.
}

}  // namespace google_apis
