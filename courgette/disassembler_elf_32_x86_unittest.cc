// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/assembly_program.h"
#include "courgette/base_test_unittest.h"
#include "courgette/disassembler_elf_32_x86.h"

class DisassemblerElf32X86Test : public BaseTest {
 public:

  void TestExe(const char* file_name,
               size_t expected_abs_count,
               size_t expected_rel_count) const;
};

void DisassemblerElf32X86Test::TestExe(const char* file_name,
                                       size_t expected_abs_count,
                                       size_t expected_rel_count) const {
  std::string file1 = FileContents(file_name);

  scoped_ptr<courgette::DisassemblerElf32X86> disassembler(
      new courgette::DisassemblerElf32X86(file1.c_str(), file1.length()));

  bool can_parse_header = disassembler->ParseHeader();
  EXPECT_TRUE(can_parse_header);
  EXPECT_TRUE(disassembler->ok());

  // The length of the disassembled value will be slightly smaller than the
  // real file, since trailing debug info is not included
  EXPECT_EQ(file1.length(), disassembler->length());

  const uint8* offset_p = disassembler->OffsetToPointer(0);
  EXPECT_EQ(reinterpret_cast<const void*>(file1.c_str()),
            reinterpret_cast<const void*>(offset_p));
  EXPECT_EQ(0x7F, offset_p[0]);
  EXPECT_EQ('E', offset_p[1]);
  EXPECT_EQ('L', offset_p[2]);
  EXPECT_EQ('F', offset_p[3]);

  courgette::AssemblyProgram* program = new courgette::AssemblyProgram();

  EXPECT_TRUE(disassembler->Disassemble(program));

  EXPECT_EQ(disassembler->Abs32Locations().size(), expected_abs_count);
  EXPECT_EQ(disassembler->Rel32Locations().size(), expected_rel_count);

  // Prove that none of the rel32 RVAs overlap with abs32 RVAs
  std::set<courgette::RVA> abs(disassembler->Abs32Locations().begin(),
                               disassembler->Abs32Locations().end());
  std::set<courgette::RVA> rel(disassembler->Rel32Locations().begin(),
                               disassembler->Rel32Locations().end());
  for (std::vector<courgette::RVA>::iterator rel32 =
        disassembler->Rel32Locations().begin();
       rel32 !=  disassembler->Rel32Locations().end();
       rel32++) {
    EXPECT_TRUE(abs.find(*rel32) == abs.end());
  }

  for (std::vector<courgette::RVA>::iterator abs32 =
        disassembler->Abs32Locations().begin();
       abs32 !=  disassembler->Abs32Locations().end();
       abs32++) {
    EXPECT_TRUE(rel.find(*abs32) == rel.end());
  }
  delete program;
}

TEST_F(DisassemblerElf32X86Test, All) {
  TestExe("elf-32-1", 200, 3441);
}
