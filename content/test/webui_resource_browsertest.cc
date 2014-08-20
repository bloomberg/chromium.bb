// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "grit/webui_resources.h"
#include "net/base/filename_util.h"

namespace content {

class WebUIResourceBrowserTest : public ContentBrowserTest {
 public:
  WebUIResourceBrowserTest() {}
  virtual ~WebUIResourceBrowserTest() {}

  // Runs all test functions in |file|, waiting for them to complete.
  void RunTest(const base::FilePath& file) {
    ASSERT_TRUE(PathExists(file));
    NavigateToURL(shell(), net::FilePathToFileURL(file));

    content::WebContents* web_contents = shell()->web_contents();
    ASSERT_TRUE(web_contents);
    EXPECT_TRUE(ExecuteWebUIResourceTest(web_contents, include_libraries_));
  }

  void RunMediaInternalsTest(const base::FilePath::CharType* file) {
    AddLibrary(IDR_WEBUI_JS_CR);
    AddLibrary(IDR_MEDIA_INTERNALS_JS);

    base::FilePath path;
    PathService::Get(DIR_TEST_DATA, &path);
    RunTest(path.Append(FILE_PATH_LITERAL("media"))
        .Append(FILE_PATH_LITERAL("webui"))
        .Append(file));
  }

  // Queues the library corresponding to |resource_id| for injection into the
  // test. The code injection is performed post-load, so any common test
  // initialization that depends on the library should be placed in a setUp
  // function.
  void AddLibrary(int resource_id) {
    include_libraries_.push_back(resource_id);
  }

 private:
  // Resource IDs for internal javascript libraries to inject into the test.
  std::vector<int> include_libraries_;

  DISALLOW_COPY_AND_ASSIGN(WebUIResourceBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, MediaInternals_Integration) {
  RunMediaInternalsTest(FILE_PATH_LITERAL("integration_test.html"));
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, MediaInternals_PlayerInfo) {
  RunMediaInternalsTest(FILE_PATH_LITERAL("player_info_test.html"));
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, MediaInternals_Manager) {
  RunMediaInternalsTest(FILE_PATH_LITERAL("manager_test.html"));
}

}  // namespace content
