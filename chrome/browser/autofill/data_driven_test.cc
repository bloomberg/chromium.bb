// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/data_driven_test.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Reads |file| into |content|, and converts Windows line-endings to Unix ones.
// Returns true on success.
bool ReadFile(const FilePath& file, std::string* content) {
  if (!file_util::ReadFileToString(file, content))
    return false;

  ReplaceSubstringsAfterOffset(content, 0, "\r\n", "\n");
  return true;
}

// Write |content| to |file|. Returns true on success.
bool WriteFile(const FilePath& file, const std::string& content) {
  int write_size = file_util::WriteFile(file, content.c_str(),
                                        content.length());
  return write_size == static_cast<int>(content.length());
}

}  // namespace

void DataDrivenTest::RunDataDrivenTest(
    const FilePath& input_directory,
    const FilePath& output_directory,
    const FilePath::StringType& file_name_pattern) {
  file_util::FileEnumerator input_files(input_directory,
                                        false,
                                        file_util::FileEnumerator::FILES,
                                        file_name_pattern);

  for (FilePath input_file = input_files.Next();
       !input_file.empty();
       input_file = input_files.Next()) {
    SCOPED_TRACE(input_file.BaseName().value());

    std::string input;
    ASSERT_TRUE(ReadFile(input_file, &input));

    std::string output;
    GenerateResults(input, &output);

    FilePath output_file = output_directory.Append(
        input_file.BaseName().StripTrailingSeparators().ReplaceExtension(
            FILE_PATH_LITERAL(".out")));

    std::string output_file_contents;
    if (ReadFile(output_file, &output_file_contents))
      EXPECT_EQ(output_file_contents, output);
    else
      ASSERT_TRUE(WriteFile(output_file, output));
  }
}

FilePath DataDrivenTest::GetInputDirectory(
    const FilePath::StringType& test_name) {
  FilePath test_data_dir_;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("autofill")
                                 .Append(test_name)
                                 .AppendASCII("input");
  return test_data_dir_;
}

FilePath DataDrivenTest::GetOutputDirectory(
    const FilePath::StringType& test_name) {
  FilePath test_data_dir_;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("autofill")
                                 .Append(test_name)
                                 .AppendASCII("output");
  return test_data_dir_;
}

DataDrivenTest::DataDrivenTest() {
}

DataDrivenTest::~DataDrivenTest() {
}
