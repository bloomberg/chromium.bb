// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/scoped_temp_dir.h"
#include "content/test/content_browser_test.h"

class GURL;
class LayoutTestHttpServer;

class InProcessBrowserLayoutTest : public content::ContentBrowserTest {
 public:
  explicit InProcessBrowserLayoutTest(const FilePath& test_parent_dir,
                                      const FilePath& test_case_dir);
  // Used when running HTTP layout tests. Starts the server in the constructor
  // and keeps it running through the lifetime of this test. This is done to
  // avoid flakiness in restarting the server while the port is still in use.
  // If -1 is passed for |port|, a random number will be used. This is
  // recommended when possible, in case multiple tests are running at the same
  // time. For some tests this isn't possible though, because they use resources
  // that hardcode a specific port.
  InProcessBrowserLayoutTest(const FilePath& test_parent_dir,
                             const FilePath& test_case_dir,
                             int port);
  virtual ~InProcessBrowserLayoutTest();

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;
  // Runs a layout test given its filename relative to the path given to the
  // constructor.
  void RunLayoutTest(const std::string& test_case_file_name);
  // Runs a layout test using the HTTP test server. The second constructor must
  // have been used.
  void RunHttpLayoutTest(const std::string& test_case_file_name);
  void AddResourceForLayoutTest(const FilePath& parent_dir,
                                const FilePath& resource_name);

 private:
  void RunLayoutTestInternal(const std::string& test_case_file_name,
                             const GURL& url);
  void WriteModifiedFile(const std::string& test_case_file_name,
                         FilePath* test_path);
  std::string SaveResults(const std::string& expected,
                          const std::string& actual);

  FilePath our_original_layout_test_dir_;
  FilePath test_parent_dir_;
  FilePath test_case_dir_;
  FilePath our_layout_test_temp_dir_;
  FilePath rebase_result_dir_;
  FilePath rebase_result_chromium_dir_;
  FilePath rebase_result_win_dir_;
  ScopedTempDir scoped_temp_dir_;
  int port_;  // -2 means no port.  -1 means random.
  scoped_ptr<LayoutTestHttpServer> test_http_server_;

  DISALLOW_COPY_AND_ASSIGN(InProcessBrowserLayoutTest);
};
