// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/path_service.h"
#include "base/file_util.h"
#include "base/string_util.h"

#include "courgette/courgette.h"
#include "courgette/streams.h"

#include "testing/gtest/include/gtest/gtest.h"

class VersioningTest : public testing::Test {
 public:
  void TestApplyingOldPatch(const char* src_file,
                         const char* patch_file,
                         const char* expected_file) const;

 private:
  void SetUp() {
    PathService::Get(base::DIR_SOURCE_ROOT, &testdata_dir_);
    testdata_dir_ = testdata_dir_.AppendASCII("courgette");
    testdata_dir_ = testdata_dir_.AppendASCII("testdata");
  }

  void TearDown() { }

  // Returns contents of |file_name| as uninterprested bytes stored in a string.
  std::string FileContents(const char* file_name) const;

  FilePath testdata_dir_;  // Full path name of testdata directory
};

//  Reads a test file into a string.
std::string VersioningTest::FileContents(const char* file_name) const {
  FilePath file_path = testdata_dir_;
  file_path = file_path.AppendASCII(file_name);
  std::string file_contents;
  EXPECT_TRUE(file_util::ReadFileToString(file_path, &file_contents));
  return file_contents;
}

void VersioningTest::TestApplyingOldPatch(const char* src_file,
                                          const char* patch_file,
                                          const char* expected_file) const {

  std::string old_buffer = FileContents(src_file);
  std::string new_buffer = FileContents(patch_file);
  std::string expected_buffer = FileContents(expected_file);

  courgette::SourceStream old_stream;
  courgette::SourceStream patch_stream;
  old_stream.Init(old_buffer);
  patch_stream.Init(new_buffer);

  courgette::SinkStream generated_stream;

  courgette::Status status =
      courgette::ApplyEnsemblePatch(&old_stream,
                                    &patch_stream,
                                    &generated_stream);

  EXPECT_EQ(status, courgette::C_OK);

  size_t expected_length = expected_buffer.size();
  size_t generated_length = generated_stream.Length();

  EXPECT_EQ(generated_length, expected_length);
  EXPECT_EQ(0, memcmp(generated_stream.Buffer(),
                      expected_buffer.c_str(),
                      expected_length));
}


TEST_F(VersioningTest, All) {
  TestApplyingOldPatch("setup1.exe", "setup1-setup2.v1.patch", "setup2.exe");

  // We also need a way to test that newly generated patches are appropriately
  // applicable by older clients... not sure of the best way to do that.
}
