// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_DATA_DRIVEN_TEST_H_
#define CHROME_BROWSER_AUTOFILL_DATA_DRIVEN_TEST_H_

#include <string>

#include "base/files/file_path.h"
#include "base/string16.h"

// A convenience class for implementing data-driven tests. Subclassers need only
// implement the conversion of serialized input data to serialized output data
// and provide a set of input files. For each input file, on the first run, a
// gold output file is generated; for subsequent runs, the test ouptut is
// compared to this gold output.
class DataDrivenTest {
 public:
  // For each file in |input_directory| whose filename matches
  // |file_name_pattern|, slurps in the file contents and calls into
  // |GenerateResults()|. If the corresponding output file already exists in
  // the |output_directory|, verifies that the results match the file contents;
  // otherwise, writes a gold result file to the |output_directory|.
  void RunDataDrivenTest(const base::FilePath& input_directory,
                         const base::FilePath& output_directory,
                         const base::FilePath::StringType& file_name_pattern);

  // Given the |input| data, generates the |output| results. The output results
  // must be stable across runs.
  // Note: The return type is |void| so that googletest |ASSERT_*| macros will
  // compile.
  virtual void GenerateResults(const std::string& input,
                               std::string* output) = 0;

  // Return |base::FilePath|s to the test input and output subdirectories
  // ../autofill/|test_name|/input and ../autofill/|test_name|/output.
  base::FilePath GetInputDirectory(const base::FilePath::StringType& test_name);
  base::FilePath GetOutputDirectory(
      const base::FilePath::StringType& test_name);

 protected:
  DataDrivenTest();
  virtual ~DataDrivenTest();

 private:
  DISALLOW_COPY_AND_ASSIGN(DataDrivenTest);
};

#endif  // CHROME_BROWSER_AUTOFILL_DATA_DRIVEN_TEST_H_
