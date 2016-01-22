// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/assembly_program.h"

#include <memory.h>
#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

#include "courgette/courgette.h"
#include "courgette/encoded_program.h"

namespace courgette {

namespace {

// Sets the current address for the emitting instructions.
class OriginInstruction : public Instruction {
 public:
  explicit OriginInstruction(RVA rva) : Instruction(ORIGIN, 0), rva_(rva) {}
  RVA origin_rva() const { return rva_; }
 private:
  RVA  rva_;
};

// Emits an entire PE base relocation table.
class PeRelocsInstruction : public Instruction {
 public:
  PeRelocsInstruction() : Instruction(MAKEPERELOCS) {}
};

// Emits an ELF relocation table.
class ElfRelocsInstruction : public Instruction {
 public:
  ElfRelocsInstruction() : Instruction(MAKEELFRELOCS) {}
};

// Emits an ELF ARM relocation table.
class ElfARMRelocsInstruction : public Instruction {
 public:
  ElfARMRelocsInstruction() : Instruction(MAKEELFARMRELOCS) {}
};

// Emits a single byte.
class ByteInstruction : public Instruction {
 public:
  explicit ByteInstruction(uint8_t value) : Instruction(DEFBYTE, value) {}
  uint8_t byte_value() const { return info_; }
};

// Emits a single byte.
class BytesInstruction : public Instruction {
 public:
  BytesInstruction(const uint8_t* values, size_t len)
      : Instruction(DEFBYTES, 0), values_(values), len_(len) {}
  const uint8_t* byte_values() const { return values_; }
  size_t len() const { return len_; }

 private:
  const uint8_t* values_;
  size_t len_;
};

// A ABS32 to REL32 instruction emits a reference to a label's address.
class InstructionWithLabel : public Instruction {
 public:
  InstructionWithLabel(OP op, Label* label)
    : Instruction(op, 0), label_(label) {
    if (label == NULL) NOTREACHED();
  }
  Label* label() const { return label_; }
 protected:
  Label* label_;
};

// An ARM REL32 instruction emits a reference to a label's address and
// a specially-compressed ARM op.
class InstructionWithLabelARM : public InstructionWithLabel {
 public:
  InstructionWithLabelARM(OP op,
                          uint16_t compressed_op,
                          Label* label,
                          const uint8_t* arm_op,
                          uint16_t op_size)
      : InstructionWithLabel(op, label),
        compressed_op_(compressed_op),
        arm_op_(arm_op),
        op_size_(op_size) {
    if (label == NULL) NOTREACHED();
  }
  uint16_t compressed_op() const { return compressed_op_; }
  const uint8_t* arm_op() const { return arm_op_; }
  uint16_t op_size() const { return op_size_; }

 private:
  uint16_t compressed_op_;
  const uint8_t* arm_op_;
  uint16_t op_size_;
};

}  // namespace

AssemblyProgram::AssemblyProgram(ExecutableType kind)
  : kind_(kind), image_base_(0) {
}

static void DeleteContainedLabels(const RVAToLabel& labels) {
  for (RVAToLabel::const_iterator p = labels.begin();  p != labels.end();  ++p)
    UncheckedDelete(p->second);
}

AssemblyProgram::~AssemblyProgram() {
  for (size_t i = 0;  i < instructions_.size();  ++i) {
    Instruction* instruction = instructions_[i];
    if (instruction->op() != DEFBYTE)  // Owned by byte_instruction_cache_.
      UncheckedDelete(instruction);
  }
  if (byte_instruction_cache_.get()) {
    for (size_t i = 0;  i < 256;  ++i)
      UncheckedDelete(byte_instruction_cache_[i]);
  }
  DeleteContainedLabels(rel32_labels_);
  DeleteContainedLabels(abs32_labels_);
}

CheckBool AssemblyProgram::EmitPeRelocsInstruction() {
  return Emit(ScopedInstruction(UncheckedNew<PeRelocsInstruction>()));
}

CheckBool AssemblyProgram::EmitElfRelocationInstruction() {
  return Emit(ScopedInstruction(UncheckedNew<ElfRelocsInstruction>()));
}

CheckBool AssemblyProgram::EmitElfARMRelocationInstruction() {
  return Emit(ScopedInstruction(UncheckedNew<ElfARMRelocsInstruction>()));
}

CheckBool AssemblyProgram::EmitOriginInstruction(RVA rva) {
  return Emit(ScopedInstruction(UncheckedNew<OriginInstruction>(rva)));
}

CheckBool AssemblyProgram::EmitByteInstruction(uint8_t byte) {
  return EmitShared(GetByteInstruction(byte));
}

CheckBool AssemblyProgram::EmitBytesInstruction(const uint8_t* values,
                                                size_t len) {
  return Emit(ScopedInstruction(UncheckedNew<BytesInstruction>(values, len)));
}

CheckBool AssemblyProgram::EmitRel32(Label* label) {
  return Emit(
      ScopedInstruction(UncheckedNew<InstructionWithLabel>(REL32, label)));
}

CheckBool AssemblyProgram::EmitRel32ARM(uint16_t op,
                                        Label* label,
                                        const uint8_t* arm_op,
                                        uint16_t op_size) {
  return Emit(ScopedInstruction(UncheckedNew<InstructionWithLabelARM>(
      REL32ARM, op, label, arm_op, op_size)));
}

CheckBool AssemblyProgram::EmitAbs32(Label* label) {
  return Emit(
      ScopedInstruction(UncheckedNew<InstructionWithLabel>(ABS32, label)));
}

CheckBool AssemblyProgram::EmitAbs64(Label* label) {
  return Emit(
      ScopedInstruction(UncheckedNew<InstructionWithLabel>(ABS64, label)));
}

Label* AssemblyProgram::FindOrMakeAbs32Label(RVA rva) {
  return FindLabel(rva, &abs32_labels_);
}

Label* AssemblyProgram::FindOrMakeRel32Label(RVA rva) {
  return FindLabel(rva, &rel32_labels_);
}

void AssemblyProgram::DefaultAssignIndexes() {
  DefaultAssignIndexes(&abs32_labels_);
  DefaultAssignIndexes(&rel32_labels_);
}

void AssemblyProgram::UnassignIndexes() {
  UnassignIndexes(&abs32_labels_);
  UnassignIndexes(&rel32_labels_);
}

void AssemblyProgram::AssignRemainingIndexes() {
  AssignRemainingIndexes(&abs32_labels_);
  AssignRemainingIndexes(&rel32_labels_);
}

Label* AssemblyProgram::InstructionAbs32Label(
    const Instruction* instruction) const {
  if (instruction->op() == ABS32)
    return static_cast<const InstructionWithLabel*>(instruction)->label();
  return NULL;
}

Label* AssemblyProgram::InstructionAbs64Label(
    const Instruction* instruction) const {
  if (instruction->op() == ABS64)
    return static_cast<const InstructionWithLabel*>(instruction)->label();
  return NULL;
}

Label* AssemblyProgram::InstructionRel32Label(
    const Instruction* instruction) const {
  if (instruction->op() == REL32 || instruction->op() == REL32ARM) {
    Label* label =
        static_cast<const InstructionWithLabel*>(instruction)->label();
    return label;
  }
  return NULL;
}

CheckBool AssemblyProgram::Emit(ScopedInstruction instruction) {
  if (!instruction || !instructions_.push_back(instruction.get()))
    return false;
  // Ownership successfully passed to instructions_.
  ignore_result(instruction.release());
  return true;
}

CheckBool AssemblyProgram::EmitShared(Instruction* instruction) {
  DCHECK(!instruction || instruction->op() == DEFBYTE);
  return instruction && instructions_.push_back(instruction);
}

Label* AssemblyProgram::FindLabel(RVA rva, RVAToLabel* labels) {
  Label*& slot = (*labels)[rva];
  if (slot == NULL) {
    slot = UncheckedNew<Label>(rva);
    if (slot == NULL)
      return NULL;
  }
  slot->count_++;
  return slot;
}

void AssemblyProgram::UnassignIndexes(RVAToLabel* labels) {
  for (RVAToLabel::iterator p = labels->begin();  p != labels->end();  ++p) {
    Label* current = p->second;
    current->index_ = Label::kNoIndex;
  }
}

// DefaultAssignIndexes takes a set of labels and assigns indexes in increasing
// address order.
//
void AssemblyProgram::DefaultAssignIndexes(RVAToLabel* labels) {
  int index = 0;
  for (RVAToLabel::iterator p = labels->begin();  p != labels->end();  ++p) {
    Label* current = p->second;
    if (current->index_ != Label::kNoIndex)
      NOTREACHED();
    current->index_ = index;
    ++index;
  }
}

// AssignRemainingIndexes assigns indexes to any addresses (labels) that are not
// yet assigned an index.
//
void AssemblyProgram::AssignRemainingIndexes(RVAToLabel* labels) {
  // An address table compresses best when each index is associated with an
  // address that is slight larger than the previous index.

  // First see which indexes have not been used.  The 'available' vector could
  // grow even bigger, but the number of addresses is a better starting size
  // than empty.
  std::vector<bool> available(labels->size(), true);
  int used = 0;

  for (RVAToLabel::iterator p = labels->begin();  p != labels->end();  ++p) {
    int index = p->second->index_;
    if (index != Label::kNoIndex) {
      while (static_cast<size_t>(index) >= available.size())
        available.push_back(true);
      available.at(index) = false;
      ++used;
    }
  }

  VLOG(1) << used << " of " << labels->size() << " labels pre-assigned";

  // Are there any unused labels that happen to be adjacent following a used
  // label?
  //
  int fill_forward_count = 0;
  Label* prev = 0;
  for (RVAToLabel::iterator p = labels->begin();  p != labels->end();  ++p) {
    Label* current = p->second;
    if (current->index_ == Label::kNoIndex) {
      int index = 0;
      if (prev  &&  prev->index_ != Label::kNoIndex)
        index = prev->index_ + 1;
      if (index < static_cast<int>(available.size()) && available.at(index)) {
        current->index_ = index;
        available.at(index) = false;
        ++fill_forward_count;
      }
    }
    prev = current;
  }

  // Are there any unused labels that happen to be adjacent preceeding a used
  // label?
  //
  int fill_backward_count = 0;
  prev = 0;
  for (RVAToLabel::reverse_iterator p = labels->rbegin();
       p != labels->rend();
       ++p) {
    Label* current = p->second;
    if (current->index_ == Label::kNoIndex) {
      int prev_index;
      if (prev)
        prev_index = prev->index_;
      else
        prev_index = static_cast<uint32_t>(available.size());
      if (prev_index != 0  &&
          prev_index != Label::kNoIndex  &&
          available.at(prev_index - 1)) {
        current->index_ = prev_index - 1;
        available.at(current->index_) = false;
        ++fill_backward_count;
      }
    }
    prev = current;
  }

  // Fill in any remaining indexes
  int fill_infill_count = 0;
  int index = 0;
  for (RVAToLabel::iterator p = labels->begin();  p != labels->end();  ++p) {
    Label* current = p->second;
    if (current->index_ == Label::kNoIndex) {
      while (!available.at(index)) {
        ++index;
      }
      current->index_ = index;
      available.at(index) = false;
      ++index;
      ++fill_infill_count;
    }
  }

  VLOG(1) << "  fill forward " << fill_forward_count
          << "  backward " << fill_backward_count
          << "  infill " << fill_infill_count;
}

EncodedProgram* AssemblyProgram::Encode() const {
  scoped_ptr<EncodedProgram> encoded(new EncodedProgram());

  encoded->set_image_base(image_base_);

  if (!encoded->DefineLabels(abs32_labels_, rel32_labels_))
    return NULL;

  for (size_t i = 0;  i < instructions_.size();  ++i) {
    Instruction* instruction = instructions_[i];

    switch (instruction->op()) {
      case ORIGIN: {
        OriginInstruction* org = static_cast<OriginInstruction*>(instruction);
        if (!encoded->AddOrigin(org->origin_rva()))
          return NULL;
        break;
      }
      case DEFBYTE: {
        uint8_t b = static_cast<ByteInstruction*>(instruction)->byte_value();
        if (!encoded->AddCopy(1, &b))
          return NULL;
        break;
      }
      case DEFBYTES: {
        const uint8_t* byte_values =
            static_cast<BytesInstruction*>(instruction)->byte_values();
        size_t len = static_cast<BytesInstruction*>(instruction)->len();

        if (!encoded->AddCopy(len, byte_values))
          return NULL;
        break;
      }
      case REL32: {
        Label* label = static_cast<InstructionWithLabel*>(instruction)->label();
        if (!encoded->AddRel32(label->index_))
          return NULL;
        break;
      }
      case REL32ARM: {
        Label* label =
            static_cast<InstructionWithLabelARM*>(instruction)->label();
        uint16_t compressed_op =
            static_cast<InstructionWithLabelARM*>(instruction)->compressed_op();
        if (!encoded->AddRel32ARM(compressed_op, label->index_))
          return NULL;
        break;
      }
      case ABS32: {
        Label* label = static_cast<InstructionWithLabel*>(instruction)->label();
        if (!encoded->AddAbs32(label->index_))
          return NULL;
        break;
      }
      case ABS64: {
        Label* label = static_cast<InstructionWithLabel*>(instruction)->label();
        if (!encoded->AddAbs64(label->index_))
          return NULL;
        break;
      }
      case MAKEPERELOCS: {
        if (!encoded->AddPeMakeRelocs(kind_))
          return NULL;
        break;
      }
      case MAKEELFRELOCS: {
        if (!encoded->AddElfMakeRelocs())
          return NULL;
        break;
      }
      case MAKEELFARMRELOCS: {
        if (!encoded->AddElfARMMakeRelocs())
          return NULL;
        break;
      }
      default: {
        NOTREACHED() << "Unknown Insn OP kind";
      }
    }
  }

  return encoded.release();
}

Instruction* AssemblyProgram::GetByteInstruction(uint8_t byte) {
  if (!byte_instruction_cache_) {
    Instruction** ram = nullptr;
    if (!base::UncheckedMalloc(sizeof(Instruction*) * 256,
                               reinterpret_cast<void**>(&ram))) {
      return nullptr;
    }
    byte_instruction_cache_.reset(ram);

    for (int i = 0; i < 256; ++i) {
      byte_instruction_cache_[i] =
          UncheckedNew<ByteInstruction>(static_cast<uint8_t>(i));
      if (!byte_instruction_cache_[i]) {
        for (int j = 0; j < i; ++j)
          UncheckedDelete(byte_instruction_cache_[j]);
        byte_instruction_cache_.reset();
        return nullptr;
      }
    }
  }

  return byte_instruction_cache_[byte];
}

// Chosen empirically to give the best reduction in payload size for
// an update from daisy_3701.98.0 to daisy_4206.0.0.
const int AssemblyProgram::kLabelLowerLimit = 5;

CheckBool AssemblyProgram::TrimLabels() {
  // For now only trim for ARM binaries.
  if (kind() != EXE_ELF_32_ARM)
    return true;

  int lower_limit = kLabelLowerLimit;

  VLOG(1) << "TrimLabels: threshold " << lower_limit;

  // Walk through the list of instructions, replacing trimmed labels
  // with the original machine instruction.
  for (size_t i = 0; i < instructions_.size(); ++i) {
    Instruction* instruction = instructions_[i];
    switch (instruction->op()) {
      case REL32ARM: {
        Label* label =
            static_cast<InstructionWithLabelARM*>(instruction)->label();
        if (label->count_ <= lower_limit) {
          const uint8_t* arm_op =
              static_cast<InstructionWithLabelARM*>(instruction)->arm_op();
          uint16_t op_size =
              static_cast<InstructionWithLabelARM*>(instruction)->op_size();

          if (op_size < 1)
            return false;
          UncheckedDelete(instruction);
          instructions_[i] = UncheckedNew<BytesInstruction>(arm_op, op_size);
          if (!instructions_[i])
            return false;
        }
        break;
      }
      default:
        break;
    }
  }

  // Remove and deallocate underused Labels.
  RVAToLabel::iterator it = rel32_labels_.begin();
  while (it != rel32_labels_.end()) {
    if (it->second->count_ <= lower_limit) {
      UncheckedDelete(it->second);
      rel32_labels_.erase(it++);
    } else {
      ++it;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////

Status Encode(AssemblyProgram* program, EncodedProgram** output) {
  *output = NULL;
  EncodedProgram *encoded = program->Encode();
  if (encoded) {
    *output = encoded;
    return C_OK;
  } else {
    return C_GENERAL_ERROR;
  }
}

}  // namespace courgette
