// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/path_service.h"
#include "chrome/browser/safe_browsing/pe_image_reader_win.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

struct TestData {
  const char* filename;
  safe_browsing::PeImageReader::WordSize word_size;
  WORD machine_identifier;
  WORD optional_header_size;
  size_t number_of_sections;
  size_t number_of_debug_entries;
};

// A test fixture parameterized on test data containing the name of a PE image
// to parse and the expected values to be read from it. The file is read from
// the src/chrome/test/data/safe_browsing directory.
class PeImageReaderTest : public testing::TestWithParam<const TestData*> {
 protected:
  PeImageReaderTest() : expected_data_(GetParam()) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_file_path_));
    data_file_path_ = data_file_path_.AppendASCII("safe_browsing");
    data_file_path_ = data_file_path_.AppendASCII(expected_data_->filename);

    ASSERT_TRUE(data_file_.Initialize(data_file_path_));

    ASSERT_TRUE(image_reader_.Initialize(data_file_.data(),
                                         data_file_.length()));
  }

  const TestData* expected_data_;
  base::FilePath data_file_path_;
  base::MemoryMappedFile data_file_;
  safe_browsing::PeImageReader image_reader_;
};

TEST_P(PeImageReaderTest, GetWordSize) {
  EXPECT_EQ(expected_data_->word_size, image_reader_.GetWordSize());
}

TEST_P(PeImageReaderTest, GetDosHeader) {
  const IMAGE_DOS_HEADER* dos_header = image_reader_.GetDosHeader();
  ASSERT_NE(reinterpret_cast<const IMAGE_DOS_HEADER*>(NULL), dos_header);
  EXPECT_EQ(IMAGE_DOS_SIGNATURE, dos_header->e_magic);
}

TEST_P(PeImageReaderTest, GetCoffFileHeader) {
  const IMAGE_FILE_HEADER* file_header = image_reader_.GetCoffFileHeader();
  ASSERT_NE(reinterpret_cast<const IMAGE_FILE_HEADER*>(NULL), file_header);
  EXPECT_EQ(expected_data_->machine_identifier, file_header->Machine);
  EXPECT_EQ(expected_data_->optional_header_size,
            file_header->SizeOfOptionalHeader);
}

TEST_P(PeImageReaderTest, GetOptionalHeaderData) {
  size_t optional_header_size = 0;
  const uint8_t* optional_header_data =
      image_reader_.GetOptionalHeaderData(&optional_header_size);
  ASSERT_NE(reinterpret_cast<const uint8_t*>(NULL), optional_header_data);
  EXPECT_EQ(expected_data_->optional_header_size, optional_header_size);
}

TEST_P(PeImageReaderTest, GetNumberOfSections) {
  EXPECT_EQ(expected_data_->number_of_sections,
            image_reader_.GetNumberOfSections());
}

TEST_P(PeImageReaderTest, GetSectionHeaderAt) {
  size_t number_of_sections = image_reader_.GetNumberOfSections();
  for (size_t i = 0; i < number_of_sections; ++i) {
    const IMAGE_SECTION_HEADER* section_header =
        image_reader_.GetSectionHeaderAt(i);
    ASSERT_NE(reinterpret_cast<const IMAGE_SECTION_HEADER*>(NULL),
              section_header);
  }
}

TEST_P(PeImageReaderTest, InitializeFailTruncatedFile) {
  // Compute the size of all headers through the section headers.
  const IMAGE_SECTION_HEADER* last_section_header =
      image_reader_.GetSectionHeaderAt(image_reader_.GetNumberOfSections() - 1);
  const uint8_t* headers_end =
      reinterpret_cast<const uint8_t*>(last_section_header) +
      sizeof(*last_section_header);
  size_t header_size = headers_end - data_file_.data();
  safe_browsing::PeImageReader short_reader;

  // Initialize should succeed when all headers are present.
  EXPECT_TRUE(short_reader.Initialize(data_file_.data(), header_size));

  // But fail if anything is missing.
  for (size_t i = 0; i < header_size; ++i) {
    EXPECT_FALSE(short_reader.Initialize(data_file_.data(), i));
  }
}

TEST_P(PeImageReaderTest, GetExportSection) {
  size_t section_size = 0;
  const uint8_t* export_section = image_reader_.GetExportSection(&section_size);
  ASSERT_NE(reinterpret_cast<const uint8_t*>(NULL), export_section);
  EXPECT_NE(0U, section_size);
}

TEST_P(PeImageReaderTest, GetNumberOfDebugEntries) {
  EXPECT_EQ(expected_data_->number_of_debug_entries,
            image_reader_.GetNumberOfDebugEntries());
}

TEST_P(PeImageReaderTest, GetDebugEntry) {
  size_t number_of_debug_entries = image_reader_.GetNumberOfDebugEntries();
  for (size_t i = 0; i < number_of_debug_entries; ++i) {
    const uint8_t* raw_data = NULL;
    size_t raw_data_size = 0;
    const IMAGE_DEBUG_DIRECTORY* entry =
        image_reader_.GetDebugEntry(i, &raw_data, &raw_data_size);
    EXPECT_NE(reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(NULL), entry);
    EXPECT_NE(reinterpret_cast<const uint8_t*>(NULL), raw_data);
    EXPECT_NE(0U, raw_data_size);
  }
}

namespace {

const TestData kTestData[] = {
  {
    "module_with_exports_x86.dll",
    safe_browsing::PeImageReader::WORD_SIZE_32,
    IMAGE_FILE_MACHINE_I386,
    sizeof(IMAGE_OPTIONAL_HEADER32),
    4,
    1,
  }, {
    "module_with_exports_x64.dll",
    safe_browsing::PeImageReader::WORD_SIZE_64,
    IMAGE_FILE_MACHINE_AMD64,
    sizeof(IMAGE_OPTIONAL_HEADER64),
    5,
    1,
  },
};

}  // namespace

INSTANTIATE_TEST_CASE_P(WordSize32,
                        PeImageReaderTest,
                        testing::Values(&kTestData[0]));
INSTANTIATE_TEST_CASE_P(WordSize64,
                        PeImageReaderTest,
                        testing::Values(&kTestData[1]));
