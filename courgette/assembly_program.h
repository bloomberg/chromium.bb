// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_ASSEMBLY_PROGRAM_H_
#define COURGETTE_ASSEMBLY_PROGRAM_H_

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

#include "courgette/disassembler.h"
#include "courgette/memory_allocator.h"

namespace courgette {

class EncodedProgram;
class Instruction;

typedef NoThrowBuffer<Instruction*> InstructionVector;

// A Label is a symbolic reference to an address.  Unlike a conventional
// assembly language, we always know the address.  The address will later be
// stored in a table and the Label will be replaced with the index into the
// table.
//
// TODO(sra): Make fields private and add setters and getters.
class Label {
 public:
  static const int kNoIndex = -1;
  Label() : rva_(0), index_(kNoIndex), count_(0) {}
  explicit Label(RVA rva) : rva_(rva), index_(kNoIndex), count_(0) {}

  RVA rva_;    // Address referred to by the label.
  int index_;  // Index of address in address table, kNoIndex until assigned.
  int count_;
};

typedef std::map<RVA, Label*> RVAToLabel;

// An AssemblyProgram is the result of disassembling an executable file.
//
// * The disassembler creates labels in the AssemblyProgram and emits
//   'Instructions'.
// * The disassembler then calls DefaultAssignIndexes to assign
//   addresses to positions in the address tables.
// * [Optional step]
// * At this point the AssemblyProgram can be converted into an
//   EncodedProgram and serialized to an output stream.
// * Later, the EncodedProgram can be deserialized and assembled into
//   the original file.
//
// The optional step is to modify the AssemblyProgram.  One form of modification
// is to assign indexes in such a way as to make the EncodedProgram for this
// AssemblyProgram look more like the EncodedProgram for some other
// AssemblyProgram.  The modification process should call UnassignIndexes, do
// its own assignment, and then call AssignRemainingIndexes to ensure all
// indexes are assigned.
//
class AssemblyProgram {
 public:
  explicit AssemblyProgram(ExecutableType kind);
  ~AssemblyProgram();

  ExecutableType kind() const { return kind_; }

  void set_image_base(uint64 image_base) { image_base_ = image_base; }

  // Instructions will be assembled in the order they are emitted.

  // Generates an entire base relocation table.
  CheckBool EmitPeRelocsInstruction() WARN_UNUSED_RESULT;

  // Generates an ELF style relocation table for X86.
  CheckBool EmitElfRelocationInstruction() WARN_UNUSED_RESULT;

  // Generates an ELF style relocation table for ARM.
  CheckBool EmitElfARMRelocationInstruction() WARN_UNUSED_RESULT;

  // Following instruction will be assembled at address 'rva'.
  CheckBool EmitOriginInstruction(RVA rva) WARN_UNUSED_RESULT;

  // Generates a single byte of data or machine instruction.
  CheckBool EmitByteInstruction(uint8 byte) WARN_UNUSED_RESULT;

  // Generates multiple bytes of data or machine instructions.
  CheckBool EmitBytesInstruction(const uint8* value, uint32 len)
      WARN_UNUSED_RESULT;

  // Generates 4-byte relative reference to address of 'label'.
  CheckBool EmitRel32(Label* label) WARN_UNUSED_RESULT;

  // Generates 4-byte relative reference to address of 'label' for
  // ARM.
  CheckBool EmitRel32ARM(uint16 op, Label* label, const uint8* arm_op,
                         uint16 op_size) WARN_UNUSED_RESULT;

  // Generates 4-byte absolute reference to address of 'label'.
  CheckBool EmitAbs32(Label* label) WARN_UNUSED_RESULT;

  // Looks up a label or creates a new one.  Might return NULL.
  Label* FindOrMakeAbs32Label(RVA rva);

  // Looks up a label or creates a new one.  Might return NULL.
  Label* FindOrMakeRel32Label(RVA rva);

  void DefaultAssignIndexes();
  void UnassignIndexes();
  void AssignRemainingIndexes();

  EncodedProgram* Encode() const;

  // Accessor for instruction list.
  const InstructionVector& instructions() const {
    return instructions_;
  }

  // Returns the label if the instruction contains and absolute address,
  // otherwise returns NULL.
  Label* InstructionAbs32Label(const Instruction* instruction) const;

  // Returns the label if the instruction contains and rel32 offset,
  // otherwise returns NULL.
  Label* InstructionRel32Label(const Instruction* instruction) const;

  // Trim underused labels
  CheckBool TrimLabels();

  void PrintLabelCounts(RVAToLabel* labels);
  void CountRel32ARM();

 private:
  ExecutableType kind_;

  CheckBool Emit(Instruction* instruction) WARN_UNUSED_RESULT;

  static const int kLabelLowerLimit;

  // Looks up a label or creates a new one.  Might return NULL.
  Label* FindLabel(RVA rva, RVAToLabel* labels);

  // Helper methods for the public versions.
  static void UnassignIndexes(RVAToLabel* labels);
  static void DefaultAssignIndexes(RVAToLabel* labels);
  static void AssignRemainingIndexes(RVAToLabel* labels);

  // Sharing instructions that emit a single byte saves a lot of space.
  Instruction* GetByteInstruction(uint8 byte);
  scoped_ptr<Instruction*[]> byte_instruction_cache_;

  uint64 image_base_;  // Desired or mandated base address of image.

  InstructionVector instructions_;  // All the instructions in program.

  // These are lookup maps to find the label associated with a given address.
  // We have separate label spaces for addresses referenced by rel32 labels and
  // abs32 labels.  This is somewhat arbitrary.
  RVAToLabel rel32_labels_;
  RVAToLabel abs32_labels_;

  DISALLOW_COPY_AND_ASSIGN(AssemblyProgram);
};

}  // namespace courgette
#endif  // COURGETTE_ASSEMBLY_PROGRAM_H_
