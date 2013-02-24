// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "content/browser/download/download_file_factory.h"
#include "content/browser/download/download_file_impl.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/drag_download_file.h"
#include "content/browser/download/drag_download_util.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/power_save_blocker.h"
#include "content/public/common/content_client.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/shell/shell_browser_context.h"
#include "content/shell/shell_download_manager_delegate.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "content/test/net/url_request_slow_download_job.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InvokeWithoutArgs;

namespace content {

class MockDownloadFileObserver : public ui::DownloadFileObserver {
 public:
  MockDownloadFileObserver() {}

  MOCK_METHOD1(OnDownloadCompleted, void(const base::FilePath& file_path));
  MOCK_METHOD0(OnDownloadAborted, void());

 private:
  virtual ~MockDownloadFileObserver() {}

  DISALLOW_COPY_AND_ASSIGN(MockDownloadFileObserver);
};

class DragDownloadFileTest : public ContentBrowserTest {
 public:
  DragDownloadFileTest() {}
  virtual ~DragDownloadFileTest() {}

  void Succeed() {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        MessageLoopForUI::current()->QuitClosure());
  }

  void FailFast() {
    CHECK(false);
  }

 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());
    ShellDownloadManagerDelegate* delegate =
        static_cast<ShellDownloadManagerDelegate*>(
            shell()->web_contents()->GetBrowserContext()
            ->GetDownloadManagerDelegate());
    delegate->SetDownloadBehaviorForTesting(downloads_directory());
  }

  void SetUpServer() {
    base::FilePath mock_base(GetTestFilePath("download", ""));
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&URLRequestMockHTTPJob::AddUrlHandler, mock_base));
  }

  const base::FilePath& downloads_directory() const {
    return downloads_directory_.path();
  }

 private:
  base::ScopedTempDir downloads_directory_;

  DISALLOW_COPY_AND_ASSIGN(DragDownloadFileTest);
};

IN_PROC_BROWSER_TEST_F(DragDownloadFileTest, DragDownloadFileTest_NetError) {
  base::FilePath name(downloads_directory().AppendASCII(
      "DragDownloadFileTest_NetError.txt"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(base::FilePath(FILE_PATH_LITERAL(
      "download-test.lib"))));
  Referrer referrer;
  std::string referrer_encoding;
  DragDownloadFile* file = new DragDownloadFile(
      name, scoped_ptr<net::FileStream>(NULL), url, referrer,
      referrer_encoding, shell()->web_contents());
  scoped_refptr<MockDownloadFileObserver> observer(
      new MockDownloadFileObserver());
  EXPECT_CALL(*observer, OnDownloadAborted()).WillOnce(InvokeWithoutArgs(
      this, &DragDownloadFileTest::Succeed));
  ON_CALL(*observer, OnDownloadCompleted(_)).WillByDefault(InvokeWithoutArgs(
      this, &DragDownloadFileTest::FailFast));
  file->Start(observer);
  RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(DragDownloadFileTest, DragDownloadFileTest_Complete) {
  base::FilePath name(downloads_directory().AppendASCII(
        "DragDownloadFileTest_Complete.txt"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(base::FilePath(FILE_PATH_LITERAL(
      "download-test.lib"))));
  Referrer referrer;
  std::string referrer_encoding;
  net::FileStream* stream = NULL;
#if defined(OS_POSIX)
  stream = CreateFileStreamForDrop(
      &name, GetContentClient()->browser()->GetNetLog());
#endif
  SetUpServer();
  DragDownloadFile* file = new DragDownloadFile(
      name, scoped_ptr<net::FileStream>(stream), url, referrer,
      referrer_encoding, shell()->web_contents());
  scoped_refptr<MockDownloadFileObserver> observer(
      new MockDownloadFileObserver());
  EXPECT_CALL(*observer, OnDownloadCompleted(_)).WillOnce(InvokeWithoutArgs(
      this, &DragDownloadFileTest::Succeed));
  ON_CALL(*observer, OnDownloadAborted()).WillByDefault(InvokeWithoutArgs(
      this, &DragDownloadFileTest::FailFast));
  file->Start(observer);
  RunMessageLoop();
}

// TODO(benjhayden): Test Stop().

}  // namespace content
