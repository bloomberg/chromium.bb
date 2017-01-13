// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/assembly_program.h"

#include "base/callback.h"
#include "base/logging.h"
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

/******** InstructionCountReceptor ********/

// An InstructionReceptor that counts space occupied by emitted instructions.
class InstructionCountReceptor : public InstructionReceptor {
 public:
  InstructionCountReceptor() = default;

  size_t size() const { return size_; }

  // InstructionReceptor:
  // TODO(huangs): 2016/11: Populate these with size_ += ...
  CheckBool EmitPeRelocs() override { return true; }
  CheckBool EmitElfRelocation() override { return true; }
  CheckBool EmitElfARMRelocation() override { return true; }
  CheckBool EmitOrigin(RVA rva) override { return true; }
  CheckBool EmitSingleByte(uint8_t byte) override { return true; }
  CheckBool EmitMultipleBytes(const uint8_t* bytes, size_t len) override {
    return true;
  }
  CheckBool EmitRel32(Label* label) override { return true; }
  CheckBool EmitRel32ARM(uint16_t op,
                         Label* label,
                         const uint8_t* arm_op,
                         uint16_t op_size) override {
    return true;
  }
  CheckBool EmitAbs32(Label* label) override { return true; }
  CheckBool EmitAbs64(Label* label) override { return true; }

 private:
  size_t size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(InstructionCountReceptor);
};

/******** InstructionStoreReceptor ********/

// An InstructionReceptor that stores emitted instructions.
class InstructionStoreReceptor : public InstructionReceptor {
 public:
  explicit InstructionStoreReceptor(AssemblyProgram* program)
      : program_(program) {
    CHECK(program_);
  }

  // TODO(huangs): 2016/11: Add Reserve().

  // InstructionReceptor:
  // TODO(huangs): 2016/11: Replace stub with implementation.
  CheckBool EmitPeRelocs() override { return program_->EmitPeRelocs(); }
  CheckBool EmitElfRelocation() override {
    return program_->EmitElfRelocation();
  }
  CheckBool EmitElfARMRelocation() override {
    return program_->EmitElfARMRelocation();
  }
  CheckBool EmitOrigin(RVA rva) override { return program_->EmitOrigin(rva); }
  CheckBool EmitSingleByte(uint8_t byte) override {
    return program_->EmitSingleByte(byte);
  }
  CheckBool EmitMultipleBytes(const uint8_t* bytes, size_t len) override {
    return program_->EmitMultipleBytes(bytes, len);
  }
  CheckBool EmitRel32(Label* label) override {
    return program_->EmitRel32(label);
  }
  CheckBool EmitRel32ARM(uint16_t op,
                         Label* label,
                         const uint8_t* arm_op,
                         uint16_t op_size) override {
    return program_->EmitRel32ARM(op, label, arm_op, op_size);
  }
  CheckBool EmitAbs32(Label* label) override {
    return program_->EmitAbs32(label);
  }
  CheckBool EmitAbs64(Label* label) override {
    return program_->EmitAbs64(label);
  }

 private:
  AssemblyProgram* program_;

  DISALLOW_COPY_AND_ASSIGN(InstructionStoreReceptor);
};

}  // namespace

/******** AssemblyProgram ********/

AssemblyProgram::AssemblyProgram(ExecutableType kind, uint64_t image_base)
    : kind_(kind), image_base_(image_base) {}

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
}

CheckBool AssemblyProgram::EmitPeRelocs() {
  return Emit(ScopedInstruction(UncheckedNew<PeRelocsInstruction>()));
}

CheckBool AssemblyProgram::EmitElfRelocation() {
  return Emit(ScopedInstruction(UncheckedNew<ElfRelocsInstruction>()));
}

CheckBool AssemblyProgram::EmitElfARMRelocation() {
  return Emit(ScopedInstruction(UncheckedNew<ElfARMRelocsInstruction>()));
}

CheckBool AssemblyProgram::EmitOrigin(RVA rva) {
  return Emit(ScopedInstruction(UncheckedNew<OriginInstruction>(rva)));
}

CheckBool AssemblyProgram::EmitSingleByte(uint8_t byte) {
  return EmitShared(GetByteInstruction(byte));
}

CheckBool AssemblyProgram::EmitMultipleBytes(const uint8_t* bytes, size_t len) {
  return Emit(ScopedInstruction(UncheckedNew<BytesInstruction>(bytes, len)));
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

void AssemblyProgram::PrecomputeLabels(RvaVisitor* abs32_visitor,
                                       RvaVisitor* rel32_visitor) {
  abs32_label_manager_.Read(abs32_visitor);
  rel32_label_manager_.Read(rel32_visitor);
  TrimLabels();
}

// Chosen empirically to give the best reduction in payload size for
// an update from daisy_3701.98.0 to daisy_4206.0.0.
const int AssemblyProgram::kLabelLowerLimit = 5;

void AssemblyProgram::TrimLabels() {
  // For now only trim for ARM binaries.
  if (kind() != EXE_ELF_32_ARM)
    return;

  int lower_limit = kLabelLowerLimit;

  VLOG(1) << "TrimLabels: threshold " << lower_limit;

  rel32_label_manager_.RemoveUnderusedLabels(lower_limit);
}

void AssemblyProgram::UnassignIndexes() {
  abs32_label_manager_.UnassignIndexes();
  rel32_label_manager_.UnassignIndexes();
}

void AssemblyProgram::DefaultAssignIndexes() {
  abs32_label_manager_.DefaultAssignIndexes();
  rel32_label_manager_.DefaultAssignIndexes();
}

void AssemblyProgram::AssignRemainingIndexes() {
  abs32_label_manager_.AssignRemainingIndexes();
  rel32_label_manager_.AssignRemainingIndexes();
}

Label* AssemblyProgram::FindAbs32Label(RVA rva) {
  return abs32_label_manager_.Find(rva);
}

Label* AssemblyProgram::FindRel32Label(RVA rva) {
  return rel32_label_manager_.Find(rva);
}

void AssemblyProgram::HandleInstructionLabels(
    const AssemblyProgram::LabelHandlerMap& handler_map) const {
  for (const Instruction* instruction : instructions_) {
    LabelHandlerMap::const_iterator it = handler_map.find(instruction->op());
    if (it != handler_map.end()) {
      it->second.Run(
          static_cast<const InstructionWithLabel*>(instruction)->label());
    }
  }
}

CheckBool AssemblyProgram::GenerateInstructions(
    const InstructionGenerator& gen) {
  // Pass 1: Count the space needed to store instructions.
  InstructionCountReceptor count_receptor;
  if (!gen.Run(this, &count_receptor))
    return false;

  // Pass 2: Emit all instructions to preallocated buffer (uses Phase 1 count).
  InstructionStoreReceptor store_receptor(this);
  // TODO(huangs): 2016/11: Pass |count_receptor_->size()| to |store_receptor_|
  // to reserve space for raw data.
  return gen.Run(this, &store_receptor);
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

std::unique_ptr<EncodedProgram> AssemblyProgram::Encode() const {
  std::unique_ptr<EncodedProgram> encoded(new EncodedProgram());

  encoded->set_image_base(image_base_);

  if (!encoded->ImportLabels(abs32_label_manager_, rel32_label_manager_))
    return nullptr;

  for (size_t i = 0;  i < instructions_.size();  ++i) {
    Instruction* instruction = instructions_[i];

    switch (instruction->op()) {
      case ORIGIN: {
        OriginInstruction* org = static_cast<OriginInstruction*>(instruction);
        if (!encoded->AddOrigin(org->origin_rva()))
          return nullptr;
        break;
      }
      case DEFBYTE: {
        uint8_t b = static_cast<ByteInstruction*>(instruction)->byte_value();
        if (!encoded->AddCopy(1, &b))
          return nullptr;
        break;
      }
      case DEFBYTES: {
        const uint8_t* byte_values =
            static_cast<BytesInstruction*>(instruction)->byte_values();
        size_t len = static_cast<BytesInstruction*>(instruction)->len();

        if (!encoded->AddCopy(len, byte_values))
          return nullptr;
        break;
      }
      case REL32: {
        Label* label = static_cast<InstructionWithLabel*>(instruction)->label();
        if (!encoded->AddRel32(label->index_))
          return nullptr;
        break;
      }
      case REL32ARM: {
        Label* label =
            static_cast<InstructionWithLabelARM*>(instruction)->label();
        uint16_t compressed_op =
            static_cast<InstructionWithLabelARM*>(instruction)->compressed_op();
        if (!encoded->AddRel32ARM(compressed_op, label->index_))
          return nullptr;
        break;
      }
      case ABS32: {
        Label* label = static_cast<InstructionWithLabel*>(instruction)->label();
        if (!encoded->AddAbs32(label->index_))
          return nullptr;
        break;
      }
      case ABS64: {
        Label* label = static_cast<InstructionWithLabel*>(instruction)->label();
        if (!encoded->AddAbs64(label->index_))
          return nullptr;
        break;
      }
      case MAKEPERELOCS: {
        if (!encoded->AddPeMakeRelocs(kind_))
          return nullptr;
        break;
      }
      case MAKEELFRELOCS: {
        if (!encoded->AddElfMakeRelocs())
          return nullptr;
        break;
      }
      case MAKEELFARMRELOCS: {
        if (!encoded->AddElfARMMakeRelocs())
          return nullptr;
        break;
      }
      default: {
        NOTREACHED() << "Unknown Insn OP kind";
      }
    }
  }

  return encoded;
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

////////////////////////////////////////////////////////////////////////////////

Status Encode(const AssemblyProgram& program,
              std::unique_ptr<EncodedProgram>* output) {
  // Explicitly release any memory associated with the output before encoding.
  output->reset();

  *output = program.Encode();
  return (*output) ? C_OK : C_GENERAL_ERROR;
}

}  // namespace courgette
