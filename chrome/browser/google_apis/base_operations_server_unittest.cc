// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/base_operations.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/google_apis/operation_registry.h"
#include "chrome/browser/google_apis/operation_runner.h"
#include "chrome/browser/google_apis/task_util.h"
#include "chrome/browser/google_apis/test_server/http_request.h"
#include "chrome/browser/google_apis/test_server/http_response.h"
#include "chrome/browser/google_apis/test_server/http_server.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kTestAuthToken[] = "testtoken";
const char kTestUserAgent[] = "test-user-agent";

}  // namespace

class BaseOperationsServerTest : public testing::Test {
 protected:
  BaseOperationsServerTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE),
        io_thread_(content::BrowserThread::IO) {
  }

  virtual void SetUp() OVERRIDE {
    file_thread_.Start();
    io_thread_.StartIOThread();
    profile_.reset(new TestingProfile);

    request_context_getter_ = new net::TestURLRequestContextGetter(
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::IO));

    ASSERT_TRUE(test_server_.InitializeAndWaitUntilReady());
    test_server_.RegisterRequestHandler(
        base::Bind(&test_util::HandleDownloadRequest,
                   test_server_.base_url(),
                   base::Unretained(&http_request_)));
  }

  virtual void TearDown() OVERRIDE {
    test_server_.ShutdownAndWaitUntilComplete();
    request_context_getter_ = NULL;
  }

  // Returns a temporary file path suitable for storing the cache file.
  base::FilePath GetTestCachedFilePath(const base::FilePath& file_name) {
    return profile_->GetPath().Append(file_name);
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;
  test_server::HttpServer test_server_;
  scoped_ptr<TestingProfile> profile_;
  OperationRegistry operation_registry_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;

  // The incoming HTTP request is saved so tests can verify the request
  // parameters like HTTP method (ex. some operations should use DELETE
  // instead of GET).
  test_server::HttpRequest http_request_;
};

TEST_F(BaseOperationsServerTest, DownloadFileOperation_ValidFile) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  base::FilePath temp_file;
  DownloadFileOperation* operation = new DownloadFileOperation(
      &operation_registry_,
      request_context_getter_.get(),
      CreateComposedCallback(
          base::Bind(&test_util::RunAndQuit),
          test_util::CreateCopyResultCallback(&result_code, &temp_file)),
      GetContentCallback(),
      test_server_.GetURL("/files/chromeos/gdata/testfile.txt"),
      base::FilePath::FromUTF8Unsafe("/dummy/gdata/testfile.txt"),
      GetTestCachedFilePath(
          base::FilePath::FromUTF8Unsafe("cached_testfile.txt")));
  operation->Start(kTestAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  std::string contents;
  file_util::ReadFileToString(temp_file, &contents);
  file_util::Delete(temp_file, false);

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/files/chromeos/gdata/testfile.txt", http_request_.relative_url);

  const base::FilePath expected_path =
      test_util::GetTestFilePath("chromeos/gdata/testfile.txt");
  std::string expected_contents;
  file_util::ReadFileToString(expected_path, &expected_contents);
  EXPECT_EQ(expected_contents, contents);
}

// http://crbug.com/169588
TEST_F(BaseOperationsServerTest,
       DISABLED_DownloadFileOperation_NonExistentFile) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  base::FilePath temp_file;
  DownloadFileOperation* operation = new DownloadFileOperation(
      &operation_registry_,
      request_context_getter_.get(),
      CreateComposedCallback(
          base::Bind(&test_util::RunAndQuit),
          test_util::CreateCopyResultCallback(&result_code, &temp_file)),
      GetContentCallback(),
      test_server_.GetURL("/files/chromeos/gdata/no-such-file.txt"),
      base::FilePath::FromUTF8Unsafe("/dummy/gdata/no-such-file.txt"),
      GetTestCachedFilePath(
          base::FilePath::FromUTF8Unsafe("cache_no-such-file.txt")));
  operation->Start(kTestAuthToken, kTestUserAgent,
                   base::Bind(&test_util::DoNothingForReAuthenticateCallback));
  MessageLoop::current()->Run();

  std::string contents;
  file_util::ReadFileToString(temp_file, &contents);
  file_util::Delete(temp_file, false);

  EXPECT_EQ(HTTP_NOT_FOUND, result_code);
  EXPECT_EQ(test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/files/chromeos/gdata/no-such-file.txt",
            http_request_.relative_url);
  // Do not verify the not found message.
}

}  // namespace google_apis
