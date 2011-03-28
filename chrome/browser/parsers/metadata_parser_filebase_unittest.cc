// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"  // TODO(brettw) remove when WideToASCII moves.
#include "chrome/browser/parsers/metadata_parser_filebase.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FileMetaDataParserTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Create a temporary directory for testing and fill it with a file.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    test_file_ = temp_dir_.path().AppendASCII("FileMetaDataParserTest");

    // Create the test file.
    std::string content = "content";
    int write_size = file_util::WriteFile(test_file_, content.c_str(),
                                          content.length());
    ASSERT_EQ(static_cast<int>(content.length()), write_size);
  }

  std::string test_file_str() {
#if defined(OS_POSIX)
    return test_file_.BaseName().value();
#elif defined(OS_WIN)
    return WideToASCII(test_file_.BaseName().value());
#endif  // defined(OS_POSIX)
  }

  std::string test_file_size() {
    int64 size;
    EXPECT_TRUE(file_util::GetFileSize(test_file_, &size));

    return base::Int64ToString(size);
  }

  ScopedTempDir temp_dir_;
  FilePath test_file_;
};

TEST_F(FileMetaDataParserTest, Parse) {
  FileMetadataParser parser(test_file_);

  EXPECT_TRUE(parser.Parse());

  std::string value;
  EXPECT_TRUE(parser.GetProperty(MetadataParser::kPropertyTitle, &value));
  EXPECT_EQ(test_file_str(), value);

  // Verify the file size property.
  EXPECT_TRUE(parser.GetProperty(MetadataParser::kPropertyFilesize, &value));
  EXPECT_EQ(test_file_size(), value);

  // FileMetadataParser does not set kPropertyType.
  EXPECT_FALSE(parser.GetProperty(MetadataParser::kPropertyType, &value));
}

TEST_F(FileMetaDataParserTest, PropertyIterator) {
  FileMetadataParser parser(test_file_);

  EXPECT_TRUE(parser.Parse());

  scoped_ptr<MetadataPropertyIterator> iter(parser.GetPropertyIterator());
  ASSERT_NE(static_cast<MetadataPropertyIterator*>(NULL), iter.get());
  ASSERT_EQ(2, iter->Length());

  std::map<std::string, std::string> expectations;
  expectations[MetadataParser::kPropertyFilesize] = test_file_size();
  expectations[MetadataParser::kPropertyTitle] = test_file_str();

  std::string key, value;
  for (int i = 0; i < iter->Length(); ++i) {
    EXPECT_FALSE(iter->IsEnd());
    EXPECT_TRUE(iter->GetNext(&key, &value));
    // No ostream operator<< implementation for map iterator, so can't use
    // ASSERT_NE.
    ASSERT_TRUE(expectations.find(key) != expectations.end());
    EXPECT_EQ(expectations[key], value);

    expectations.erase(key);
  }

  EXPECT_TRUE(iter->IsEnd());
  EXPECT_FALSE(iter->GetNext(&key, &value));
  EXPECT_TRUE(expectations.empty());
}

}  // namespace
