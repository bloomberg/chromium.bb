// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_download_helper.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "chrome/common/chrome_paths.h"
#include "content/test/test_browser_thread.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  TestURLRequestContextGetter()
      : io_message_loop_proxy_(base::MessageLoopProxy::current()) {
  }

  virtual net::URLRequestContext* GetURLRequestContext() {
    if (!context_)
      context_ = new TestURLRequestContext();
    return context_;
  }
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const {
    return io_message_loop_proxy_;
  }

 protected:
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

 private:
  virtual ~TestURLRequestContextGetter() {}

  scoped_refptr<net::URLRequestContext> context_;
};

}  // namespace

// This class provides functionality to test the plugin installer download
// file functionality.
class PluginInstallerDownloadTest : public testing::Test {
 public:
  PluginInstallerDownloadTest()
      : message_loop_(MessageLoop::TYPE_IO),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        download_helper_(NULL),
        success_(false) {}
  ~PluginInstallerDownloadTest() {}

  void Start() {
    FilePath path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    initial_download_path_ = net::FilePathToFileURL(
        path.AppendASCII("download-test1.lib"));
    download_helper_ = new PluginDownloadUrlHelper();
    TestURLRequestContextGetter* context_getter =
        new TestURLRequestContextGetter;
    download_helper_->InitiateDownload(
        initial_download_path_,
        context_getter,
        base::Bind(&PluginInstallerDownloadTest::OnDownloadCompleted,
                   base::Unretained(this)),
        base::Bind(&PluginInstallerDownloadTest::OnDownloadError,
                   base::Unretained(this)));

    message_loop_.PostDelayedTask(
        FROM_HERE, MessageLoop::QuitClosure(),
        TestTimeouts::action_max_timeout_ms());
  }

  void OnDownloadCompleted(const FilePath& download_path) {
    success_ = true;
    final_download_path_ = download_path;
    message_loop_.Quit();
    download_helper_ = NULL;
  }

  void OnDownloadError(const std::string& error) {
    ADD_FAILURE() << error;
    message_loop_.Quit();
    download_helper_ = NULL;
  }

  FilePath final_download_path() const {
    return final_download_path_;
  }

  FilePath initial_download_path() const {
    return final_download_path_;
  }

  bool success() const {
    return success_;
  }

 private:
  MessageLoop message_loop_;
  content::TestBrowserThread file_thread_;
  FilePath final_download_path_;
  PluginDownloadUrlHelper* download_helper_;
  bool success_;
  GURL initial_download_path_;
};

// This test validates that the plugin downloader downloads the specified file
// to a temporary path with the same file name.
TEST_F(PluginInstallerDownloadTest, PluginInstallerDownloadPathTest) {
  Start();
  MessageLoop::current()->Run();

  ASSERT_TRUE(success());
  EXPECT_TRUE(initial_download_path().BaseName().value() ==
              final_download_path().BaseName().value());
}
