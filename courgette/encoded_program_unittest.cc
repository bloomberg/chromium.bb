// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/encoded_program.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "courgette/disassembler.h"
#include "courgette/streams.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using courgette::EncodedProgram;

struct AddressSpec {
  int32 index;
  courgette::RVA rva;
};

// Creates a simple new program with given addresses. The orders of elements
// in |abs32_specs| and |rel32_specs| are important.
scoped_ptr<EncodedProgram> CreateTestProgram(AddressSpec* abs32_specs,
                                             size_t num_abs32_specs,
                                             AddressSpec* rel32_specs,
                                             size_t num_rel32_specs) {
  scoped_ptr<EncodedProgram> program(new EncodedProgram());

  uint32 base = 0x00900000;
  program->set_image_base(base);

  for (size_t i = 0; i < num_abs32_specs; ++i) {
    EXPECT_TRUE(program->DefineAbs32Label(abs32_specs[i].index,
                                          abs32_specs[i].rva));
  }
  for (size_t i = 0; i < num_rel32_specs; ++i) {
    EXPECT_TRUE(program->DefineRel32Label(rel32_specs[i].index,
                                          rel32_specs[i].rva));
  }
  program->EndLabels();

  EXPECT_TRUE(program->AddOrigin(0));  // Start at base.
  for (size_t i = 0; i < num_abs32_specs; ++i)
    EXPECT_TRUE(program->AddAbs32(abs32_specs[i].index));
  for (size_t i = 0; i < num_rel32_specs; ++i)
    EXPECT_TRUE(program->AddRel32(rel32_specs[i].index));
  return program;
}

bool CompareSink(const uint8 expected[],
                 size_t num_expected,
                 courgette::SinkStream* ss) {
  size_t n = ss->Length();
  if (num_expected != n)
    return false;
  const uint8* buffer = ss->Buffer();
  return memcmp(&expected[0], buffer, n) == 0;
}

}  // namespace

// Create a simple program with a few addresses and references and
// check that the bits produced are as expected.
TEST(EncodedProgramTest, Test) {
  // ABS32 index 7 == base + 4.
  AddressSpec abs32_specs[] = {{7, 4}};
  // REL32 index 5 == base + 0.
  AddressSpec rel32_specs[] = {{5, 0}};
  scoped_ptr<EncodedProgram> program(
      CreateTestProgram(abs32_specs, arraysize(abs32_specs),
                        rel32_specs, arraysize(rel32_specs)));

  // Serialize and deserialize.

  courgette::SinkStreamSet sinks;
  EXPECT_TRUE(program->WriteTo(&sinks));
  program.reset();

  courgette::SinkStream sink;
  bool can_collect = sinks.CopyTo(&sink);
  EXPECT_TRUE(can_collect);

  const void* buffer = sink.Buffer();
  size_t length = sink.Length();

  courgette::SourceStreamSet sources;
  bool can_get_source_streams = sources.Init(buffer, length);
  EXPECT_TRUE(can_get_source_streams);

  scoped_ptr<EncodedProgram> encoded2(new EncodedProgram());
  bool can_read = encoded2->ReadFrom(&sources);
  EXPECT_TRUE(can_read);

  // Finally, try to assemble.
  courgette::SinkStream assembled;
  bool can_assemble = encoded2->AssembleTo(&assembled);
  EXPECT_TRUE(can_assemble);
  encoded2.reset();

  const uint8 golden[] = {
    0x04, 0x00, 0x90, 0x00,  // ABS32 to base + 4
    0xF8, 0xFF, 0xFF, 0xFF   // REL32 from next line to base + 2
  };
  EXPECT_TRUE(CompareSink(golden, arraysize(golden), &assembled));
}

// A larger test with multiple addresses. We encode the program and check the
// contents of the address streams.
TEST(EncodedProgramTest, TestWriteAddress) {
  // Absolute addresses by index: [_, _, _, 2, _, 23, _, 11].
  AddressSpec abs32_specs[] = {{7, 11}, {3, 2}, {5, 23}};
  // Relative addresses by index: [16, 7, _, 32].
  AddressSpec rel32_specs[] = {{0, 16}, {3, 32}, {1, 7}};
  scoped_ptr<EncodedProgram> program(
      CreateTestProgram(abs32_specs, arraysize(abs32_specs),
                        rel32_specs, arraysize(rel32_specs)));

  courgette::SinkStreamSet sinks;
  EXPECT_TRUE(program->WriteTo(&sinks));
  program.reset();

  // Check addresses in sinks.
  const uint8 golden_abs32_indexes[] = {
    0x03, 0x07, 0x03, 0x05  // 3 indexes: [7, 3, 5].
  };
  EXPECT_TRUE(CompareSink(golden_abs32_indexes,
                          arraysize(golden_abs32_indexes),
                          sinks.stream(courgette::kStreamAbs32Indexes)));

  const uint8 golden_rel32_indexes[] = {
    0x03, 0x00, 0x03, 0x01  // 3 indexes: [0, 3, 1].
  };
  EXPECT_TRUE(CompareSink(golden_rel32_indexes,
                          arraysize(golden_rel32_indexes),
                          sinks.stream(courgette::kStreamRel32Indexes)));

  // Addresses: [_, _, _, 2, _, 23, _, 11].
  // Padded:    [0, 0, 0, 2, 2, 23, 23, 11].
  // Delta:     [0, 0, 0, 2, 0, 21, 0, -12].
  // Hex:       [0, 0, 0, 0x02, 0, 0x15, 0, 0xFFFFFFF4].
  // Complement neg:  [0, 0, 0, 0x02, 0, 0x15, 0, (0x0B)].
  // Varint32 Signed: [0, 0, 0, 0x04, 0, 0x2A, 0, 0x17].
  const uint8 golden_abs32_addresses[] = {
    0x08,  // 8 address deltas.
    0x00, 0x00, 0x00, 0x04, 0x00, 0x2A, 0x00, 0x17,
  };
  EXPECT_TRUE(CompareSink(golden_abs32_addresses,
                          arraysize(golden_abs32_addresses),
                          sinks.stream(courgette::kStreamAbs32Addresses)));

  // Addresses: [16, 7, _, 32].
  // Padded:    [16, 7, 7, 32].
  // Delta:     [16, -9, 0, 25].
  // Hex:       [0x10, 0xFFFFFFF7, 0, 0x19].
  // Complement Neg:  [0x10, (0x08), 0, 0x19].
  // Varint32 Signed: [0x20, 0x11, 0, 0x32].
  const uint8 golden_rel32_addresses[] = {
    0x04,  // 4 address deltas.
    0x20, 0x11, 0x00, 0x32,
  };
  EXPECT_TRUE(CompareSink(golden_rel32_addresses,
                          arraysize(golden_rel32_addresses),
                          sinks.stream(courgette::kStreamRel32Addresses)));
}
