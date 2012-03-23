// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/scoped_temp_dir.h"
#include "chrome/test/base/in_process_browser_test.h"

class InProcessBrowserLayoutTest : public InProcessBrowserTest {
 public:
  explicit InProcessBrowserLayoutTest(const FilePath relative_layout_test_path);
  virtual ~InProcessBrowserLayoutTest();

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;
  void RunLayoutTest(const std::string& test_case_file_name);
  void AddResourceForLayoutTest(const FilePath& parent_dir,
                                const FilePath& resource_name);

 private:
  void WriteModifiedFile(const std::string& test_case_file_name,
                         GURL* test_url);

  FilePath our_original_layout_test_dir_;
  FilePath original_relative_path_;
  FilePath our_layout_test_temp_dir_;
  ScopedTempDir scoped_temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(InProcessBrowserLayoutTest);
};
