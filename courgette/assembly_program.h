// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_ASSEMBLY_PROGRAM_H_
#define COURGETTE_ASSEMBLY_PROGRAM_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "courgette/disassembler.h"
#include "courgette/image_utils.h"
#include "courgette/label_manager.h"
#include "courgette/memory_allocator.h"

namespace courgette {

class EncodedProgram;

// Opcodes of simple assembly language
enum OP {
  ORIGIN,         // ORIGIN <rva> - set current address for assembly.
  MAKEPERELOCS,   // Generates a base relocation table.
  MAKEELFRELOCS,  // Generates a base relocation table.
  DEFBYTE,        // DEFBYTE <value> - emit a byte literal.
  REL32,          // REL32 <label> - emit a rel32 encoded reference to 'label'.
  ABS32,          // ABS32 <label> - emit an abs32 encoded reference to 'label'.
  REL32ARM,       // REL32ARM <c_op> <label> - arm-specific rel32 reference
  MAKEELFARMRELOCS,  // Generates a base relocation table.
  DEFBYTES,       // Emits any number of byte literals
  ABS64,          // ABS64 <label> - emit an abs64 encoded reference to 'label'.
  LAST_OP
};

// Base class for instructions.  Because we have so many instructions we want to
// keep them as small as possible.  For this reason we avoid virtual functions.
class Instruction {
 public:
  OP op() const { return static_cast<OP>(op_); }

 protected:
  explicit Instruction(OP op) : op_(op), info_(0) {}
  Instruction(OP op, unsigned int info) : op_(op), info_(info) {}

  uint32_t op_ : 4;     // A few bits to store the OP code.
  uint32_t info_ : 28;  // Remaining bits in first word available to subclass.

 private:
  DISALLOW_COPY_AND_ASSIGN(Instruction);
};

typedef NoThrowBuffer<Instruction*> InstructionVector;

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

  void set_image_base(uint64_t image_base) { image_base_ = image_base; }

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
  CheckBool EmitByteInstruction(uint8_t byte) WARN_UNUSED_RESULT;

  // Generates multiple bytes of data or machine instructions.
  CheckBool EmitBytesInstruction(const uint8_t* value,
                                 size_t len) WARN_UNUSED_RESULT;

  // Generates 4-byte relative reference to address of 'label'.
  CheckBool EmitRel32(Label* label) WARN_UNUSED_RESULT;

  // Generates 4-byte relative reference to address of 'label' for
  // ARM.
  CheckBool EmitRel32ARM(uint16_t op,
                         Label* label,
                         const uint8_t* arm_op,
                         uint16_t op_size) WARN_UNUSED_RESULT;

  // Generates 4-byte absolute reference to address of 'label'.
  CheckBool EmitAbs32(Label* label) WARN_UNUSED_RESULT;

  // Generates 8-byte absolute reference to address of 'label'.
  CheckBool EmitAbs64(Label* label) WARN_UNUSED_RESULT;

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

  // Returns the label if the instruction contains an absolute 32-bit address,
  // otherwise returns NULL.
  Label* InstructionAbs32Label(const Instruction* instruction) const;

  // Returns the label if the instruction contains an absolute 64-bit address,
  // otherwise returns NULL.
  Label* InstructionAbs64Label(const Instruction* instruction) const;

  // Returns the label if the instruction contains a rel32 offset,
  // otherwise returns NULL.
  Label* InstructionRel32Label(const Instruction* instruction) const;

  // Removes underused Labels. Thresholds used (may be 0, i.e., no trimming) is
  // dependent on architecture. Returns true on success, and false otherwise.
  CheckBool TrimLabels();

 private:
  using ScopedInstruction =
      scoped_ptr<Instruction, UncheckedDeleter<Instruction>>;

  ExecutableType kind_;

  CheckBool Emit(ScopedInstruction instruction) WARN_UNUSED_RESULT;
  CheckBool EmitShared(Instruction* instruction) WARN_UNUSED_RESULT;

  static const int kLabelLowerLimit;

  // Looks up a label or creates a new one.  Might return NULL.
  Label* FindLabel(RVA rva, RVAToLabel* labels);

  // Helper methods for the public versions.
  static void UnassignIndexes(RVAToLabel* labels);
  static void DefaultAssignIndexes(RVAToLabel* labels);
  static void AssignRemainingIndexes(RVAToLabel* labels);

  // Sharing instructions that emit a single byte saves a lot of space.
  Instruction* GetByteInstruction(uint8_t byte);
  scoped_ptr<Instruction* [], base::FreeDeleter> byte_instruction_cache_;

  uint64_t image_base_;  // Desired or mandated base address of image.

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
