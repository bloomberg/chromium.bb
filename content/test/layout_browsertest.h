// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "content/test/content_browser_test.h"

class GURL;

namespace content {
class LayoutTestHttpServer;
class WebKitTestController;

class InProcessBrowserLayoutTest : public ContentBrowserTest {
 public:
  explicit InProcessBrowserLayoutTest(const base::FilePath& test_parent_dir,
                                      const base::FilePath& test_case_dir);
  // Used when running HTTP layout tests. Starts the server in the constructor
  // and keeps it running through the lifetime of this test. This is done to
  // avoid flakiness in restarting the server while the port is still in use.
  // If -1 is passed for |port|, a random number will be used. This is
  // recommended when possible, in case multiple tests are running at the same
  // time. For some tests this isn't possible though, because they use resources
  // that hardcode a specific port.
  InProcessBrowserLayoutTest(const base::FilePath& test_parent_dir,
                             const base::FilePath& test_case_dir,
                             int port);
  virtual ~InProcessBrowserLayoutTest();

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
  // Runs a layout test given its filename relative to the path given to the
  // constructor.
  void RunLayoutTest(const std::string& test_case_file_name);
  // Runs a layout test using the HTTP test server. The second constructor must
  // have been used.
  void RunHttpLayoutTest(const std::string& test_case_file_name);

 private:
  void RunLayoutTestInternal(const std::string& test_case_file_name,
                             const GURL& url);
  std::string SaveResults(const std::string& expected,
                          const std::string& actual);

  base::FilePath layout_test_dir_;
  base::FilePath test_parent_dir_;
  base::FilePath test_case_dir_;
  base::FilePath rebase_result_dir_;
  base::FilePath rebase_result_chromium_dir_;
  base::FilePath rebase_result_win_dir_;
  int port_;  // -2 means no port.  -1 means random.
  scoped_ptr<LayoutTestHttpServer> test_http_server_;

  scoped_ptr<WebKitTestController> test_controller_;

  DISALLOW_COPY_AND_ASSIGN(InProcessBrowserLayoutTest);
};

}  // namespace content
