// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/base_test_unittest.h"
#include "courgette/courgette.h"
#include "courgette/streams.h"

class EncodeDecodeTest : public BaseTest {
 public:
  void TestAssembleToStreamDisassemble(std::string file,
                                       size_t expected_encoded_lenth) const;
};

void EncodeDecodeTest::TestAssembleToStreamDisassemble(
    std::string file,
    size_t expected_encoded_lenth) const {
  const void* original_buffer = file.c_str();
  size_t original_length = file.length();

  courgette::AssemblyProgram* program = NULL;
  const courgette::Status parse_status =
      courgette::ParseDetectedExecutable(original_buffer,
                                         original_length,
                                         &program);
  EXPECT_EQ(courgette::C_OK, parse_status);

  courgette::EncodedProgram* encoded = NULL;

  const courgette::Status encode_status = Encode(program, &encoded);
  EXPECT_EQ(courgette::C_OK, encode_status);

  DeleteAssemblyProgram(program);

  courgette::SinkStreamSet sinks;
  const courgette::Status write_status = WriteEncodedProgram(encoded, &sinks);
  EXPECT_EQ(courgette::C_OK, write_status);

  DeleteEncodedProgram(encoded);

  courgette::SinkStream sink;
  bool can_collect = sinks.CopyTo(&sink);
  EXPECT_TRUE(can_collect);

  const void* buffer = sink.Buffer();
  size_t length = sink.Length();

  EXPECT_EQ(expected_encoded_lenth, length);

  courgette::SourceStreamSet sources;
  bool can_get_source_streams = sources.Init(buffer, length);
  EXPECT_TRUE(can_get_source_streams);

  courgette::EncodedProgram *encoded2 = NULL;
  const courgette::Status read_status = ReadEncodedProgram(&sources, &encoded2);
  EXPECT_EQ(courgette::C_OK, read_status);

  courgette::SinkStream assembled;
  const courgette::Status assemble_status = Assemble(encoded2, &assembled);
  EXPECT_EQ(courgette::C_OK, assemble_status);

  const void* assembled_buffer = assembled.Buffer();
  size_t assembled_length = assembled.Length();

  EXPECT_EQ(original_length, assembled_length);
  EXPECT_EQ(0, memcmp(original_buffer, assembled_buffer, original_length));

  DeleteEncodedProgram(encoded2);
}

TEST_F(EncodeDecodeTest, PE) {
  std::string file = FileContents("setup1.exe");
  TestAssembleToStreamDisassemble(file, 971850);
}

TEST_F(EncodeDecodeTest, PE64) {
  std::string file = FileContents("chrome64_1.exe");
  TestAssembleToStreamDisassemble(file, 814709);
}

TEST_F(EncodeDecodeTest, Elf_Small) {
  std::string file = FileContents("elf-32-1");
  TestAssembleToStreamDisassemble(file, 135988);
}
