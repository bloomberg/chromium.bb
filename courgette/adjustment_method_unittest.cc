// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "courgette/assembly_program.h"
#include "courgette/courgette.h"
#include "courgette/encoded_program.h"
#include "courgette/image_utils.h"
#include "courgette/streams.h"

#include "testing/gtest/include/gtest/gtest.h"

class AdjustmentMethodTest : public testing::Test {
 public:
  void Test1() const;

 private:
  void SetUp() {
  }

  void TearDown() {
  }

  // Returns one of two similar simple programs. These differ only in Label
  // assignment, so it is possible to make them look identical.
  std::unique_ptr<courgette::AssemblyProgram> MakeProgram(int kind) const {
    std::unique_ptr<courgette::AssemblyProgram> prog(
        new courgette::AssemblyProgram(courgette::EXE_WIN_32_X86));
    prog->set_image_base(0x00400000);

    courgette::RVA kRvaA = 0x00410000;
    courgette::RVA kRvaB = 0x00410004;

    std::vector<courgette::RVA> abs32_rvas;
    abs32_rvas.push_back(kRvaA);
    abs32_rvas.push_back(kRvaB);
    std::vector<courgette::RVA> rel32_rvas;  // Stub.

    courgette::TrivialRvaVisitor abs32_visitor(abs32_rvas);
    courgette::TrivialRvaVisitor rel32_visitor(rel32_rvas);
    prog->PrecomputeLabels(&abs32_visitor, &rel32_visitor);

    courgette::Label* labelA = prog->FindAbs32Label(kRvaA);
    courgette::Label* labelB = prog->FindAbs32Label(kRvaB);

    EXPECT_TRUE(prog->EmitAbs32(labelA));
    EXPECT_TRUE(prog->EmitAbs32(labelA));
    EXPECT_TRUE(prog->EmitAbs32(labelB));
    EXPECT_TRUE(prog->EmitAbs32(labelA));
    EXPECT_TRUE(prog->EmitAbs32(labelA));
    EXPECT_TRUE(prog->EmitAbs32(labelB));

    if (kind == 0) {
      labelA->index_ = 0;
      labelB->index_ = 1;
    } else {
      labelA->index_ = 1;
      labelB->index_ = 0;
    }
    prog->AssignRemainingIndexes();

    return prog;
  }

  std::unique_ptr<courgette::AssemblyProgram> MakeProgramA() const {
    return MakeProgram(0);
  }
  std::unique_ptr<courgette::AssemblyProgram> MakeProgramB() const {
    return MakeProgram(1);
  }

  // Returns a string that is the serialized version of |program|.
  // Deletes |program|.
  std::string Serialize(
      std::unique_ptr<courgette::AssemblyProgram> program) const {
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

    return std::string(reinterpret_cast<const char *>(sink.Buffer()),
                       sink.Length());
  }
};

void AdjustmentMethodTest::Test1() const {
  std::unique_ptr<courgette::AssemblyProgram> prog1 = MakeProgramA();
  std::unique_ptr<courgette::AssemblyProgram> prog2 = MakeProgramB();
  std::string s1 = Serialize(std::move(prog1));
  std::string s2 = Serialize(std::move(prog2));

  // Don't use EXPECT_EQ because strings are unprintable.
  EXPECT_FALSE(s1 == s2);  // Unadjusted A and B differ.

  std::unique_ptr<courgette::AssemblyProgram> prog5 = MakeProgramA();
  std::unique_ptr<courgette::AssemblyProgram> prog6 = MakeProgramB();
  courgette::Status can_adjust = Adjust(*prog5, prog6.get());
  EXPECT_EQ(courgette::C_OK, can_adjust);
  std::string s5 = Serialize(std::move(prog5));
  std::string s6 = Serialize(std::move(prog6));

  EXPECT_TRUE(s1 == s5);  // Adjustment did not change A (prog5)
  EXPECT_TRUE(s5 == s6);  // Adjustment did change B into A
}

TEST_F(AdjustmentMethodTest, All) {
  Test1();
}
