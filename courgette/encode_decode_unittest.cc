// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "courgette/assembly_program.h"
#include "courgette/base_test_unittest.h"
#include "courgette/courgette.h"
#include "courgette/encoded_program.h"
#include "courgette/program_detector.h"
#include "courgette/streams.h"

class EncodeDecodeTest : public BaseTest {
 public:
  void TestAssembleToStreamDisassemble(const std::string& file,
                                       size_t expected_encoded_length) const;
};

void EncodeDecodeTest::TestAssembleToStreamDisassemble(
    const std::string& file,
    size_t expected_encoded_length) const {
  const uint8_t* original_buffer =
      reinterpret_cast<const uint8_t*>(file.data());
  size_t original_length = file.length();

  std::unique_ptr<courgette::AssemblyProgram> program;
  const courgette::Status parse_status = courgette::ParseDetectedExecutable(
      original_buffer, original_length, &program);
  EXPECT_EQ(courgette::C_OK, parse_status);

  std::unique_ptr<courgette::EncodedProgram> encoded;
  const courgette::Status encode_status = Encode(*program, &encoded);
  EXPECT_EQ(courgette::C_OK, encode_status);

  program.reset();

  courgette::SinkStreamSet sinks;
  const courgette::Status write_status =
      WriteEncodedProgram(encoded.get(), &sinks);
  EXPECT_EQ(courgette::C_OK, write_status);

  encoded.reset();

  courgette::SinkStream sink;
  bool can_collect = sinks.CopyTo(&sink);
  EXPECT_TRUE(can_collect);

  const void* buffer = sink.Buffer();
  size_t length = sink.Length();

  EXPECT_EQ(expected_encoded_length, length);

  courgette::SourceStreamSet sources;
  bool can_get_source_streams = sources.Init(buffer, length);
  EXPECT_TRUE(can_get_source_streams);

  std::unique_ptr<courgette::EncodedProgram> encoded2;
  const courgette::Status read_status = ReadEncodedProgram(&sources, &encoded2);
  EXPECT_EQ(courgette::C_OK, read_status);

  courgette::SinkStream assembled;
  const courgette::Status assemble_status =
      Assemble(encoded2.get(), &assembled);
  EXPECT_EQ(courgette::C_OK, assemble_status);

  encoded2.reset();

  const void* assembled_buffer = assembled.Buffer();
  size_t assembled_length = assembled.Length();

  EXPECT_EQ(original_length, assembled_length);
  EXPECT_EQ(0, memcmp(original_buffer, assembled_buffer, original_length));
}

TEST_F(EncodeDecodeTest, PE) {
  std::string file = FileContents("setup1.exe");
  TestAssembleToStreamDisassemble(file, 972845);
}

TEST_F(EncodeDecodeTest, PE64) {
  std::string file = FileContents("chrome64_1.exe");
  TestAssembleToStreamDisassemble(file, 810090);
}

TEST_F(EncodeDecodeTest, Elf_Small) {
  std::string file = FileContents("elf-32-1");
  TestAssembleToStreamDisassemble(file, 136201);
}

TEST_F(EncodeDecodeTest, Elf_HighBSS) {
  std::string file = FileContents("elf-32-high-bss");
  TestAssembleToStreamDisassemble(file, 7308);
}

TEST_F(EncodeDecodeTest, Elf_Arm) {
  std::string file = FileContents("elf-armv7");
  TestAssembleToStreamDisassemble(file, 8531);
}
