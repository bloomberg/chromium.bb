// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/base_test_unittest.h"
#include "courgette/disassembler_elf_32_arm.h"
#include "courgette/disassembler_elf_32_x86.h"

class TypedRVATest : public BaseTest {
 public:
  void TestRelativeTargetX86(courgette::RVA word, courgette::RVA expected)
    const;

  void TestRelativeTargetARM(courgette::ARM_RVA arm_rva,
                             courgette::RVA rva,
                             uint32 op,
                             courgette::RVA expected) const;
};

void TypedRVATest::TestRelativeTargetX86(courgette::RVA word,
                                         courgette::RVA expected) const {
  courgette::DisassemblerElf32X86::TypedRVAX86* typed_rva
    = new courgette::DisassemblerElf32X86::TypedRVAX86(0);
  const uint8* op_pointer = reinterpret_cast<const uint8*>(&word);

  EXPECT_TRUE(typed_rva->ComputeRelativeTarget(op_pointer));
  EXPECT_EQ(typed_rva->relative_target(), expected);
}

uint32 Read32LittleEndian(const void* address) {
  return *reinterpret_cast<const uint32*>(address);
}

void TypedRVATest::TestRelativeTargetARM(courgette::ARM_RVA arm_rva,
                                         courgette::RVA rva,
                                         uint32 op,
                                         courgette::RVA expected) const {
  courgette::DisassemblerElf32ARM::TypedRVAARM* typed_rva
    = new courgette::DisassemblerElf32ARM::TypedRVAARM(arm_rva, 0);
  uint8* op_pointer = reinterpret_cast<uint8*>(&op);

  EXPECT_TRUE(typed_rva->ComputeRelativeTarget(op_pointer));
  EXPECT_EQ(rva + typed_rva->relative_target(), expected);
}

TEST_F(TypedRVATest, TestX86) {
  TestRelativeTargetX86(0x0, 0x4);
}

// ARM opcodes taken from and tested against the output of
// "arm-linux-gnueabi-objdump -d daisy_3701.98.0/bin/ls"

TEST_F(TypedRVATest, TestARM_OFF8_PREFETCH) {
  TestRelativeTargetARM(courgette::ARM_OFF8, 0x0, 0x0, 0x4);
}

TEST_F(TypedRVATest, TestARM_OFF8_FORWARDS) {
  TestRelativeTargetARM(courgette::ARM_OFF8, 0x2bcc, 0xd00e, 0x2bec);
  TestRelativeTargetARM(courgette::ARM_OFF8, 0x3752, 0xd910, 0x3776);
}

TEST_F(TypedRVATest, TestARM_OFF8_BACKWARDS) {
  TestRelativeTargetARM(courgette::ARM_OFF8, 0x3774, 0xd1f6, 0x3764);
}

TEST_F(TypedRVATest, TestARM_OFF11_PREFETCH) {
  TestRelativeTargetARM(courgette::ARM_OFF11, 0x0, 0x0, 0x4);
}

TEST_F(TypedRVATest, TestARM_OFF11_FORWARDS) {
  TestRelativeTargetARM(courgette::ARM_OFF11, 0x2bea, 0xe005, 0x2bf8);
}

TEST_F(TypedRVATest, TestARM_OFF11_BACKWARDS) {
  TestRelativeTargetARM(courgette::ARM_OFF11, 0x2f80, 0xe6cd, 0x2d1e);
  TestRelativeTargetARM(courgette::ARM_OFF11, 0x3610, 0xe56a, 0x30e8);
}

TEST_F(TypedRVATest, TestARM_OFF24_PREFETCH) {
  TestRelativeTargetARM(courgette::ARM_OFF24, 0x0, 0x0, 0x8);
}

TEST_F(TypedRVATest, TestARM_OFF24_FORWARDS) {
  TestRelativeTargetARM(courgette::ARM_OFF24, 0x2384, 0x4af3613a, 0xffcda874);
  TestRelativeTargetARM(courgette::ARM_OFF24, 0x23bc, 0x6af961b9, 0xffe5aaa8);
  TestRelativeTargetARM(courgette::ARM_OFF24, 0x23d4, 0x2b006823, 0x1c468);
}

TEST_F(TypedRVATest, TestARM_OFF24_BACKWARDS) {
  // TODO(paulgazz): find a real-world example of an ARM branch op
  // that jumps backwards.
}
