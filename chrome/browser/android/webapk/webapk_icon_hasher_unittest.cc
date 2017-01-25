// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_icon_hasher.h"

#include <string>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const base::FilePath::CharType kTestDataDir[] =
    FILE_PATH_LITERAL("chrome/test/data");

const char kIconUrl[] = "/android/google.png";

// Murmur2 hash for |kIconUrl|.
const char kIconMurmur2Hash[] = "2081059568551351877";

// Runs WebApkIconHasher and blocks till the murmur2 hash is computed.
class WebApkIconHasherRunner {
 public:
  WebApkIconHasherRunner()
      : url_request_context_getter_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())) {}
  ~WebApkIconHasherRunner() {}

  void Run(const GURL& icon_url) {
    WebApkIconHasher hasher;
    hasher.DownloadAndComputeMurmur2Hash(
        url_request_context_getter_.get(), icon_url,
        base::Bind(&WebApkIconHasherRunner::OnCompleted,
                   base::Unretained(this)));

    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  const std::string& murmur2_hash() { return murmur2_hash_; }

 private:
  void OnCompleted(const std::string& murmur2_hash) {
    murmur2_hash_ = murmur2_hash;
    on_completed_callback_.Run();
  }

  scoped_refptr<net::TestURLRequestContextGetter>
      url_request_context_getter_;

  // Called once the Murmur2 hash is taken.
  base::Closure on_completed_callback_;

  // Computed Murmur2 hash.
  std::string murmur2_hash_;

  DISALLOW_COPY_AND_ASSIGN(WebApkIconHasherRunner);
};

}  // anonymous namespace

class WebApkIconHasherTest : public ::testing::Test {
 public:
  WebApkIconHasherTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~WebApkIconHasherTest() override {}

  void SetUp() override {
    test_server_.AddDefaultHandlers(base::FilePath(kTestDataDir));
    ASSERT_TRUE(test_server_.Start());
  }

  net::test_server::EmbeddedTestServer* test_server() { return &test_server_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  net::EmbeddedTestServer test_server_;

  DISALLOW_COPY_AND_ASSIGN(WebApkIconHasherTest);
};

TEST_F(WebApkIconHasherTest, Success) {
  GURL icon_url = test_server()->GetURL(kIconUrl);
  WebApkIconHasherRunner runner;
  runner.Run(icon_url);
  EXPECT_EQ(kIconMurmur2Hash, runner.murmur2_hash());
}

TEST_F(WebApkIconHasherTest, DataUri) {
  GURL icon_url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA"
      "AAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO"
      "9TXL0Y4OHwAAAABJRU5ErkJggg==");
  WebApkIconHasherRunner runner;
  runner.Run(icon_url);
  EXPECT_EQ("536500236142107998", runner.murmur2_hash());
}

TEST_F(WebApkIconHasherTest, DataUriInvalid) {
  GURL icon_url("data:image/png;base64");
  WebApkIconHasherRunner runner;
  runner.Run(icon_url);
  EXPECT_EQ("", runner.murmur2_hash());
}

// Test that the hash callback is called with an empty string if an HTTP error
// prevents the icon URL from being fetched.
TEST_F(WebApkIconHasherTest, HTTPError) {
  GURL icon_url = test_server()->GetURL("/nocontent");
  WebApkIconHasherRunner runner;
  runner.Run(icon_url);
  EXPECT_EQ("", runner.murmur2_hash());
}
