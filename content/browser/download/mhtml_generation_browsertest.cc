// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ContainsRegex;
using testing::HasSubstr;

namespace content {

class MHTMLGenerationTest : public ContentBrowserTest {
 public:
  MHTMLGenerationTest() : has_mhtml_callback_run_(false), file_size_(0) {}

  void MHTMLGenerated(base::Closure quit_closure, int64 size) {
    has_mhtml_callback_run_ = true;
    file_size_ = size;
    quit_closure.Run();
  }

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ContentBrowserTest::SetUp();
  }

  bool has_mhtml_callback_run() const { return has_mhtml_callback_run_; }
  int64 file_size() const { return file_size_; }

  base::ScopedTempDir temp_dir_;

 private:
  bool has_mhtml_callback_run_;
  int64 file_size_;
};

// Tests that generating a MHTML does create contents.
// Note that the actual content of the file is not tested, the purpose of this
// test is to ensure we were successfull in creating the MHTML data from the
// renderer.
IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest, GenerateMHTML) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  base::FilePath path(temp_dir_.path());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  NavigateToURL(shell(), embedded_test_server()->GetURL("/simple_page.html"));

  base::RunLoop run_loop;
  shell()->web_contents()->GenerateMHTML(
      path, base::Bind(&MHTMLGenerationTest::MHTMLGenerated, this,
                       run_loop.QuitClosure()));

  // Block until the MHTML is generated.
  run_loop.Run();

  EXPECT_TRUE(has_mhtml_callback_run());
  EXPECT_GT(file_size(), 0);

  // Make sure the actual generated file has some contents.
  int64 file_size;
  ASSERT_TRUE(base::GetFileSize(path, &file_size));
  EXPECT_GT(file_size, 100);
}

IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest, InvalidPath) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  base::FilePath path(FILE_PATH_LITERAL("/invalid/file/path"));

  NavigateToURL(shell(), embedded_test_server()->GetURL("/simple_page.html"));

  base::RunLoop run_loop;
  shell()->web_contents()->GenerateMHTML(
      path, base::Bind(&MHTMLGenerationTest::MHTMLGenerated, this,
                       run_loop.QuitClosure()));

  // Block until the MHTML is generated.
  run_loop.Run();

  EXPECT_TRUE(has_mhtml_callback_run());
  EXPECT_EQ(file_size(), -1);  // Expecting failure.
}

// Test suite that allows testing --site-per-process against cross-site frames.
// See http://dev.chromium.org/developers/design-documents/site-isolation.
class MHTMLGenerationSitePerProcessTest : public MHTMLGenerationTest {
 public:
  MHTMLGenerationSitePerProcessTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MHTMLGenerationTest::SetUpCommandLine(command_line);

    // Append --site-per-process flag.
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    MHTMLGenerationTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
    content::SetupCrossSiteRedirector(embedded_test_server());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MHTMLGenerationSitePerProcessTest);
};

// Test for crbug.com/538766.
// Disabled because the test will fail until the bug is fixed
// (but note that the test only fails with --site-per-process flag).
IN_PROC_BROWSER_TEST_F(MHTMLGenerationSitePerProcessTest,
                       DISABLED_GenerateMHTML) {
  base::FilePath path(temp_dir_.path());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  GURL url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_one_frame.html"));
  NavigateToURL(shell(), url);

  base::RunLoop run_loop;
  shell()->web_contents()->GenerateMHTML(
      path, base::Bind(&MHTMLGenerationTest::MHTMLGenerated, this,
                       run_loop.QuitClosure()));

  // Block until the MHTML is generated.
  run_loop.Run();

  EXPECT_TRUE(has_mhtml_callback_run());
  EXPECT_GT(file_size(), 0);

  std::string mhtml;
  ASSERT_TRUE(base::ReadFileToString(path, &mhtml));

  // Make sure the contents of both frames are present.
  EXPECT_THAT(mhtml, HasSubstr("This page has one cross-site iframe"));
  EXPECT_THAT(mhtml, HasSubstr("This page has no title"));  // From title1.html.

  // Make sure that URLs of both frames are present
  // (note that these are single-line regexes).
  EXPECT_THAT(
      mhtml,
      ContainsRegex("Content-Location:.*/frame_tree/page_with_one_frame.html"));
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location:.*/title1.html"));
}

}  // namespace content
