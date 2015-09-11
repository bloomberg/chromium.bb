// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_TEST_INFO_EXTRACTOR_H_
#define COMPONENTS_TEST_RUNNER_TEST_INFO_EXTRACTOR_H_

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "components/test_runner/test_runner_export.h"
#include "url/gurl.h"

namespace test_runner {

struct TEST_RUNNER_EXPORT TestInfo {
  TestInfo(const GURL& url,
           bool enable_pixel_dumping,
           const std::string& expected_pixel_hash,
           const base::FilePath& current_working_directory);
  ~TestInfo();

  GURL url;
  bool enable_pixel_dumping;
  std::string expected_pixel_hash;
  base::FilePath current_working_directory;
};

class TEST_RUNNER_EXPORT TestInfoExtractor {
 public:
  explicit TestInfoExtractor(const base::CommandLine::StringVector& cmd_args);
  ~TestInfoExtractor();

  scoped_ptr<TestInfo> GetNextTest();

 private:
  base::CommandLine::StringVector cmdline_args_;
  size_t cmdline_position_;

  DISALLOW_COPY_AND_ASSIGN(TestInfoExtractor);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_TEST_INFO_EXTRACTOR_H_
