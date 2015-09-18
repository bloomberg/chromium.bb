// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/rel32_finder_win32_x86.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "base/macros.h"
#include "courgette/base_test_unittest.h"
#include "courgette/image_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace courgette {

namespace {

// Helper class to load and execute a Rel32FinderWin32X86 test case.
class Rel32FinderWin32X86TestCase {
 public:
  Rel32FinderWin32X86TestCase(const std::string& test_data)
      : text_start_rva_(0),
        text_end_rva_(0),
        relocs_start_rva_(0),
        relocs_end_rva_(0),
        image_end_rva_(0) {
    LoadTestFromString(test_data);
  }

  void RunTestBasic(std::string name) {
    Rel32FinderWin32X86_Basic finder(relocs_start_rva_, relocs_end_rva_,
                                     image_end_rva_);
    ASSERT_FALSE(text_data_.empty());
    finder.Find(&text_data_[0], &text_data_[0] + text_data_.size(),
        text_start_rva_, text_end_rva_, abs32_locations_);
    std::vector<RVA> rel32_locations;
    finder.SwapRel32Locations(&rel32_locations);
    EXPECT_EQ(expected_rel32_locations_, rel32_locations)
        << "From test case " << name << " (addresses are in hex)";
  }

 private:
  RVA text_start_rva_;
  RVA text_end_rva_;
  RVA relocs_start_rva_;
  RVA relocs_end_rva_;
  RVA image_end_rva_;
  std::vector<uint8> text_data_;
  std::vector<RVA> abs32_locations_;
  std::vector<RVA> expected_rel32_locations_;

  // Scans |iss| for the next non-empty line, after removing "#"-style comments
  // and stripping trailing spaces. On success, returns true and writes the
  // result to |line_out|. Otherwise returns false.
  bool ReadNonEmptyLine(std::istringstream& iss, std::string* line_out) {
    std::string line;
    while (std::getline(iss, line)) {
      // Trim comments and trailing spaces.
      size_t end_pos = std::min(line.find("#"), line.length());
      while (end_pos > 0 && line[end_pos] == ' ')
        --end_pos;
      line.resize(end_pos);
      if (!line.empty())
        break;
    }
    if (line.empty())
      return false;
    line_out->swap(line);
    return true;
  }

  // Scans |iss| for the next non-empty line, and reads (hex) uint32 into |v|.
  // Returns true iff successful.
  bool ReadHexUInt32(std::istringstream& iss, uint32* v) {
    std::string line;
    if (!ReadNonEmptyLine(iss, &line))
      return false;
    return sscanf(line.c_str(), "%X", v) == 1;
  }

  // Initializes the test case by parsing the multi-line string |test_data|
  // to extract Rel32FinderWin32X86 parameters, and read expected values.
  void LoadTestFromString(const std::string& test_data) {
    // The first lines (ignoring empty ones) specify RVA bounds.
    std::istringstream iss(test_data);
    ASSERT_TRUE(ReadHexUInt32(iss, &text_start_rva_));
    ASSERT_TRUE(ReadHexUInt32(iss, &text_end_rva_));
    ASSERT_TRUE(ReadHexUInt32(iss, &relocs_start_rva_));
    ASSERT_TRUE(ReadHexUInt32(iss, &relocs_end_rva_));
    ASSERT_TRUE(ReadHexUInt32(iss, &image_end_rva_));

    std::string line;
    // The Program section specifies instruction bytes. We require lines to be
    // formatted in "DUMPBIN /DISASM" style, i.e.,
    // "00401003: E8 00 00 00 00     call        00401008"
    //            ^  ^  ^  ^  ^  ^
    // We extract up to 6 bytes per line. The remaining are ignored.
    const int kBytesBegin = 12;
    const int kBytesEnd = 17;
    ReadNonEmptyLine(iss, &line);
    ASSERT_EQ("Program:", line);
    while (ReadNonEmptyLine(iss, &line) && line != "Abs32:") {
      std::string toks = line.substr(kBytesBegin, kBytesEnd);
      uint32 vals[6];
      int num_read = sscanf(toks.c_str(), "%X %X %X %X %X %X", &vals[0],
          &vals[1], &vals[2], &vals[3], &vals[4], &vals[5]);
      for (int i = 0; i < num_read; ++i)
        text_data_.push_back(static_cast<uint8>(vals[i] & 0xFF));
    }
    ASSERT_FALSE(text_data_.empty());

    // The Abs32 section specifies hex RVAs, one per line.
    ASSERT_EQ("Abs32:", line);
    while (ReadNonEmptyLine(iss, &line) && line != "Expected:") {
      RVA abs32_location;
      ASSERT_EQ(1, sscanf(line.c_str(), "%X", &abs32_location));
      abs32_locations_.push_back(abs32_location);
    }

    // The Expected section specifies hex Rel32 RVAs, one per line.
    ASSERT_EQ("Expected:", line);
    while (ReadNonEmptyLine(iss, &line)) {
      RVA rel32_location;
      ASSERT_EQ(1, sscanf(line.c_str(), "%X", &rel32_location));
      expected_rel32_locations_.push_back(rel32_location);
    }
  }
};

class Rel32FinderWin32X86Test : public BaseTest {
 public:
  void RunTest(const char* test_case_file) {
    Rel32FinderWin32X86TestCase test_case(FileContents(test_case_file));
    test_case.RunTestBasic(test_case_file);
  }
};

TEST_F(Rel32FinderWin32X86Test, TestBasic) {
  RunTest("rel32_win32_x86_01.txt");
  RunTest("rel32_win32_x86_02.txt");
  RunTest("rel32_win32_x86_03.txt");
  RunTest("rel32_win32_x86_04.txt");
}

}  // namespace

}  // namespace courgette
