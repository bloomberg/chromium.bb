// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/encoded_program.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/environment.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "courgette/courgette.h"
#include "courgette/disassembler_elf_32_arm.h"
#include "courgette/streams.h"
#include "courgette/types_elf.h"

namespace courgette {

// Stream indexes.
const int kStreamMisc    = 0;
const int kStreamOps     = 1;
const int kStreamBytes   = 2;
const int kStreamAbs32Indexes = 3;
const int kStreamRel32Indexes = 4;
const int kStreamAbs32Addresses = 5;
const int kStreamRel32Addresses = 6;
const int kStreamCopyCounts = 7;
const int kStreamOriginAddresses = kStreamMisc;

const int kStreamLimit = 9;

// Constructor is here rather than in the header.  Although the constructor
// appears to do nothing it is fact quite large because of the implicit calls to
// field constructors.  Ditto for the destructor.
EncodedProgram::EncodedProgram() : image_base_(0) {}
EncodedProgram::~EncodedProgram() {}

// Serializes a vector of integral values using Varint32 coding.
template<typename V>
CheckBool WriteVector(const V& items, SinkStream* buffer) {
  size_t count = items.size();
  bool ok = buffer->WriteSizeVarint32(count);
  for (size_t i = 0; ok && i < count;  ++i) {
    COMPILE_ASSERT(sizeof(items[0]) <= sizeof(uint32),  // NOLINT
                   T_must_fit_in_uint32);
    ok = buffer->WriteSizeVarint32(items[i]);
  }
  return ok;
}

template<typename V>
bool ReadVector(V* items, SourceStream* buffer) {
  uint32 count;
  if (!buffer->ReadVarint32(&count))
    return false;

  items->clear();

  bool ok = items->reserve(count);
  for (size_t i = 0;  ok && i < count;  ++i) {
    uint32 item;
    ok = buffer->ReadVarint32(&item);
    if (ok)
      ok = items->push_back(static_cast<typename V::value_type>(item));
  }

  return ok;
}

// Serializes a vector, using delta coding followed by Varint32 coding.
template<typename V>
CheckBool WriteU32Delta(const V& set, SinkStream* buffer) {
  size_t count = set.size();
  bool ok = buffer->WriteSizeVarint32(count);
  uint32 prev = 0;
  for (size_t i = 0;  ok && i < count;  ++i) {
    uint32 current = set[i];
    uint32 delta = current - prev;
    ok = buffer->WriteVarint32(delta);
    prev = current;
  }
  return ok;
}

template <typename V>
static CheckBool ReadU32Delta(V* set, SourceStream* buffer) {
  uint32 count;

  if (!buffer->ReadVarint32(&count))
    return false;

  set->clear();
  bool ok = set->reserve(count);
  uint32 prev = 0;

  for (size_t i = 0; ok && i < count;  ++i) {
    uint32 delta;
    ok = buffer->ReadVarint32(&delta);
    if (ok) {
      uint32 current = prev + delta;
      ok = set->push_back(current);
      prev = current;
    }
  }

  return ok;
}

// Write a vector as the byte representation of the contents.
//
// (This only really makes sense for a type T that has sizeof(T)==1, otherwise
// serialized representation is not endian-agnostic.  But it is useful to keep
// the possibility of a greater size for experiments comparing Varint32 encoding
// of a vector of larger integrals vs a plain form.)
//
template<typename V>
CheckBool WriteVectorU8(const V& items, SinkStream* buffer) {
  size_t count = items.size();
  bool ok = buffer->WriteSizeVarint32(count);
  if (count != 0 && ok) {
    size_t byte_count = count * sizeof(typename V::value_type);
    ok = buffer->Write(static_cast<const void*>(&items[0]), byte_count);
  }
  return ok;
}

template<typename V>
bool ReadVectorU8(V* items, SourceStream* buffer) {
  uint32 count;
  if (!buffer->ReadVarint32(&count))
    return false;

  items->clear();
  bool ok = items->resize(count, 0);
  if (ok && count != 0) {
    size_t byte_count = count * sizeof(typename V::value_type);
    return buffer->Read(static_cast<void*>(&((*items)[0])), byte_count);
  }
  return ok;
}

////////////////////////////////////////////////////////////////////////////////

CheckBool EncodedProgram::DefineRel32Label(int index, RVA value) {
  return DefineLabelCommon(&rel32_rva_, index, value);
}

CheckBool EncodedProgram::DefineAbs32Label(int index, RVA value) {
  return DefineLabelCommon(&abs32_rva_, index, value);
}

static const RVA kUnassignedRVA = static_cast<RVA>(-1);

CheckBool EncodedProgram::DefineLabelCommon(RvaVector* rvas,
                                            int index,
                                            RVA rva) {
  bool ok = true;
  if (static_cast<int>(rvas->size()) <= index)
    ok = rvas->resize(index + 1, kUnassignedRVA);

  if (ok) {
    DCHECK_EQ((*rvas)[index], kUnassignedRVA)
        << "DefineLabel double assigned " << index;
    (*rvas)[index] = rva;
  }

  return ok;
}

void EncodedProgram::EndLabels() {
  FinishLabelsCommon(&abs32_rva_);
  FinishLabelsCommon(&rel32_rva_);
}

void EncodedProgram::FinishLabelsCommon(RvaVector* rvas) {
  // Replace all unassigned slots with the value at the previous index so they
  // delta-encode to zero.  (There might be better values than zero.  The way to
  // get that is have the higher level assembly program assign the unassigned
  // slots.)
  RVA previous = 0;
  size_t size = rvas->size();
  for (size_t i = 0;  i < size;  ++i) {
    if ((*rvas)[i] == kUnassignedRVA)
      (*rvas)[i] = previous;
    else
      previous = (*rvas)[i];
  }
}

CheckBool EncodedProgram::AddOrigin(RVA origin) {
  return ops_.push_back(ORIGIN) && origins_.push_back(origin);
}

CheckBool EncodedProgram::AddCopy(uint32 count, const void* bytes) {
  const uint8* source = static_cast<const uint8*>(bytes);

  bool ok = true;

  // Fold adjacent COPY instructions into one.  This nearly halves the size of
  // an EncodedProgram with only COPY1 instructions since there are approx plain
  // 16 bytes per reloc.  This has a working-set benefit during decompression.
  // For compression of files with large differences this makes a small (4%)
  // improvement in size.  For files with small differences this degrades the
  // compressed size by 1.3%
  if (!ops_.empty()) {
    if (ops_.back() == COPY1) {
      ops_.back() = COPY;
      ok = copy_counts_.push_back(1);
    }
    if (ok && ops_.back() == COPY) {
      copy_counts_.back() += count;
      for (uint32 i = 0; ok && i < count; ++i) {
        ok = copy_bytes_.push_back(source[i]);
      }
      return ok;
    }
  }

  if (ok) {
    if (count == 1) {
      ok = ops_.push_back(COPY1) && copy_bytes_.push_back(source[0]);
    } else {
      ok = ops_.push_back(COPY) && copy_counts_.push_back(count);
      for (uint32 i = 0; ok && i < count; ++i) {
        ok = copy_bytes_.push_back(source[i]);
      }
    }
  }

  return ok;
}

CheckBool EncodedProgram::AddAbs32(int label_index) {
  return ops_.push_back(ABS32) && abs32_ix_.push_back(label_index);
}

CheckBool EncodedProgram::AddRel32(int label_index) {
  return ops_.push_back(REL32) && rel32_ix_.push_back(label_index);
}

CheckBool EncodedProgram::AddRel32ARM(uint16 op, int label_index) {
  return ops_.push_back(static_cast<OP>(op)) &&
      rel32_ix_.push_back(label_index);
}

CheckBool EncodedProgram::AddPeMakeRelocs(ExecutableType kind) {
  if (kind == EXE_WIN_32_X86)
    return ops_.push_back(MAKE_PE_RELOCATION_TABLE);
  return ops_.push_back(MAKE_PE64_RELOCATION_TABLE);
}

CheckBool EncodedProgram::AddElfMakeRelocs() {
  return ops_.push_back(MAKE_ELF_RELOCATION_TABLE);
}

CheckBool EncodedProgram::AddElfARMMakeRelocs() {
  return ops_.push_back(MAKE_ELF_ARM_RELOCATION_TABLE);
}

void EncodedProgram::DebuggingSummary() {
  VLOG(1) << "EncodedProgram Summary"
          << "\n  image base  " << image_base_
          << "\n  abs32 rvas  " << abs32_rva_.size()
          << "\n  rel32 rvas  " << rel32_rva_.size()
          << "\n  ops         " << ops_.size()
          << "\n  origins     " << origins_.size()
          << "\n  copy_counts " << copy_counts_.size()
          << "\n  copy_bytes  " << copy_bytes_.size()
          << "\n  abs32_ix    " << abs32_ix_.size()
          << "\n  rel32_ix    " << rel32_ix_.size();
}

////////////////////////////////////////////////////////////////////////////////

// For algorithm refinement purposes it is useful to write subsets of the file
// format.  This gives us the ability to estimate the entropy of the
// differential compression of the individual streams, which can provide
// invaluable insights.  The default, of course, is to include all the streams.
//
enum FieldSelect {
  INCLUDE_ABS32_ADDRESSES = 0x0001,
  INCLUDE_REL32_ADDRESSES = 0x0002,
  INCLUDE_ABS32_INDEXES   = 0x0010,
  INCLUDE_REL32_INDEXES   = 0x0020,
  INCLUDE_OPS             = 0x0100,
  INCLUDE_BYTES           = 0x0200,
  INCLUDE_COPY_COUNTS     = 0x0400,
  INCLUDE_MISC            = 0x1000
};

static FieldSelect GetFieldSelect() {
#if 1
  // TODO(sra): Use better configuration.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string s;
  env->GetVar("A_FIELDS", &s);
  if (!s.empty()) {
    return static_cast<FieldSelect>(
        wcstoul(base::ASCIIToWide(s).c_str(), 0, 0));
  }
#endif
  return  static_cast<FieldSelect>(~0);
}

CheckBool EncodedProgram::WriteTo(SinkStreamSet* streams) {
  FieldSelect select = GetFieldSelect();

  // The order of fields must be consistent in WriteTo and ReadFrom, regardless
  // of the streams used.  The code can be configured with all kStreamXXX
  // constants the same.
  //
  // If we change the code to pipeline reading with assembly (to avoid temporary
  // storage vectors by consuming operands directly from the stream) then we
  // need to read the base address and the random access address tables first,
  // the rest can be interleaved.

  if (select & INCLUDE_MISC) {
    // TODO(sra): write 64 bits.
    if (!streams->stream(kStreamMisc)->WriteVarint32(
            static_cast<uint32>(image_base_))) {
      return false;
    }
  }

  bool success = true;

  if (select & INCLUDE_ABS32_ADDRESSES) {
    success &= WriteU32Delta(abs32_rva_,
                             streams->stream(kStreamAbs32Addresses));
  }

  if (select & INCLUDE_REL32_ADDRESSES) {
    success &= WriteU32Delta(rel32_rva_,
                             streams->stream(kStreamRel32Addresses));
  }

  if (select & INCLUDE_MISC)
    success &= WriteVector(origins_, streams->stream(kStreamOriginAddresses));

  if (select & INCLUDE_OPS) {
    // 5 for length.
    success &= streams->stream(kStreamOps)->Reserve(ops_.size() + 5);
    success &= WriteVector(ops_, streams->stream(kStreamOps));
  }

  if (select & INCLUDE_COPY_COUNTS)
    success &= WriteVector(copy_counts_, streams->stream(kStreamCopyCounts));

  if (select & INCLUDE_BYTES)
    success &= WriteVectorU8(copy_bytes_, streams->stream(kStreamBytes));

  if (select & INCLUDE_ABS32_INDEXES)
    success &= WriteVector(abs32_ix_, streams->stream(kStreamAbs32Indexes));

  if (select & INCLUDE_REL32_INDEXES)
    success &= WriteVector(rel32_ix_, streams->stream(kStreamRel32Indexes));

  return success;
}

bool EncodedProgram::ReadFrom(SourceStreamSet* streams) {
  // TODO(sra): read 64 bits.
  uint32 temp;
  if (!streams->stream(kStreamMisc)->ReadVarint32(&temp))
    return false;
  image_base_ = temp;

  if (!ReadU32Delta(&abs32_rva_, streams->stream(kStreamAbs32Addresses)))
    return false;
  if (!ReadU32Delta(&rel32_rva_, streams->stream(kStreamRel32Addresses)))
    return false;
  if (!ReadVector(&origins_, streams->stream(kStreamOriginAddresses)))
    return false;
  if (!ReadVector(&ops_, streams->stream(kStreamOps)))
    return false;
  if (!ReadVector(&copy_counts_, streams->stream(kStreamCopyCounts)))
    return false;
  if (!ReadVectorU8(&copy_bytes_, streams->stream(kStreamBytes)))
    return false;
  if (!ReadVector(&abs32_ix_, streams->stream(kStreamAbs32Indexes)))
    return false;
  if (!ReadVector(&rel32_ix_, streams->stream(kStreamRel32Indexes)))
    return false;

  // Check that streams have been completely consumed.
  for (int i = 0;  i < kStreamLimit;  ++i) {
    if (streams->stream(i)->Remaining() > 0)
      return false;
  }

  return true;
}

// Safe, non-throwing version of std::vector::at().  Returns 'true' for success,
// 'false' for out-of-bounds index error.
template<typename V, typename T>
bool VectorAt(const V& v, size_t index, T* output) {
  if (index >= v.size())
    return false;
  *output = v[index];
  return true;
}

CheckBool EncodedProgram::EvaluateRel32ARM(OP op,
                                           size_t& ix_rel32_ix,
                                           RVA& current_rva,
                                           SinkStream* output) {
  switch (op & 0x0000F000) {
    case REL32ARM8: {
      uint32 index;
      if (!VectorAt(rel32_ix_, ix_rel32_ix, &index))
        return false;
      ++ix_rel32_ix;
      RVA rva;
      if (!VectorAt(rel32_rva_, index, &rva))
        return false;
      uint32 decompressed_op;
      if (!DisassemblerElf32ARM::Decompress(ARM_OFF8,
                                            static_cast<uint16>(op),
                                            static_cast<uint32>(rva -
                                                                current_rva),
                                            &decompressed_op)) {
        return false;
      }
      uint16 op16 = decompressed_op;
      if (!output->Write(&op16, 2))
        return false;
      current_rva += 2;
      break;
    }
    case REL32ARM11: {
      uint32 index;
      if (!VectorAt(rel32_ix_, ix_rel32_ix, &index))
        return false;
      ++ix_rel32_ix;
      RVA rva;
      if (!VectorAt(rel32_rva_, index, &rva))
        return false;
      uint32 decompressed_op;
      if (!DisassemblerElf32ARM::Decompress(ARM_OFF11, (uint16) op,
                                            (uint32) (rva - current_rva),
                                            &decompressed_op)) {
        return false;
      }
      uint16 op16 = decompressed_op;
      if (!output->Write(&op16, 2))
        return false;
      current_rva += 2;
      break;
    }
    case REL32ARM24: {
      uint32 index;
      if (!VectorAt(rel32_ix_, ix_rel32_ix, &index))
        return false;
      ++ix_rel32_ix;
      RVA rva;
      if (!VectorAt(rel32_rva_, index, &rva))
        return false;
      uint32 decompressed_op;
      if (!DisassemblerElf32ARM::Decompress(ARM_OFF24, (uint16) op,
                                            (uint32) (rva - current_rva),
                                            &decompressed_op)) {
        return false;
      }
      if (!output->Write(&decompressed_op, 4))
        return false;
      current_rva += 4;
      break;
    }
    case REL32ARM25: {
      uint32 index;
      if (!VectorAt(rel32_ix_, ix_rel32_ix, &index))
        return false;
      ++ix_rel32_ix;
      RVA rva;
      if (!VectorAt(rel32_rva_, index, &rva))
        return false;
      uint32 decompressed_op;
      if (!DisassemblerElf32ARM::Decompress(ARM_OFF25, (uint16) op,
                                            (uint32) (rva - current_rva),
                                            &decompressed_op)) {
        return false;
      }
      uint32 words = (decompressed_op << 16) | (decompressed_op >> 16);
      if (!output->Write(&words, 4))
        return false;
      current_rva += 4;
      break;
    }
    case REL32ARM21: {
      uint32 index;
      if (!VectorAt(rel32_ix_, ix_rel32_ix, &index))
        return false;
      ++ix_rel32_ix;
      RVA rva;
      if (!VectorAt(rel32_rva_, index, &rva))
        return false;
      uint32 decompressed_op;
      if (!DisassemblerElf32ARM::Decompress(ARM_OFF21, (uint16) op,
                                            (uint32) (rva - current_rva),
                                            &decompressed_op)) {
        return false;
      }
      uint32 words = (decompressed_op << 16) | (decompressed_op >> 16);
      if (!output->Write(&words, 4))
        return false;
      current_rva += 4;
      break;
    }
    default:
      return false;
  }

  return true;
}

CheckBool EncodedProgram::AssembleTo(SinkStream* final_buffer) {
  // For the most part, the assembly process walks the various tables.
  // ix_mumble is the index into the mumble table.
  size_t ix_origins = 0;
  size_t ix_copy_counts = 0;
  size_t ix_copy_bytes = 0;
  size_t ix_abs32_ix = 0;
  size_t ix_rel32_ix = 0;

  RVA current_rva = 0;

  bool pending_pe_relocation_table = false;
  uint8 pending_pe_relocation_table_type = 0x03;  // IMAGE_REL_BASED_HIGHLOW
  Elf32_Word pending_elf_relocation_table_type = 0;
  SinkStream bytes_following_relocation_table;

  SinkStream* output = final_buffer;

  for (size_t ix_ops = 0;  ix_ops < ops_.size();  ++ix_ops) {
    OP op = ops_[ix_ops];

    switch (op) {
      default:
        if (!EvaluateRel32ARM(op, ix_rel32_ix, current_rva, output))
          return false;
        break;

      case ORIGIN: {
        RVA section_rva;
        if (!VectorAt(origins_, ix_origins, &section_rva))
          return false;
        ++ix_origins;
        current_rva = section_rva;
        break;
      }

      case COPY: {
        uint32 count;
        if (!VectorAt(copy_counts_, ix_copy_counts, &count))
          return false;
        ++ix_copy_counts;
        for (uint32 i = 0;  i < count;  ++i) {
          uint8 b;
          if (!VectorAt(copy_bytes_, ix_copy_bytes, &b))
            return false;
          ++ix_copy_bytes;
          if (!output->Write(&b, 1))
            return false;
        }
        current_rva += count;
        break;
      }

      case COPY1: {
        uint8 b;
        if (!VectorAt(copy_bytes_, ix_copy_bytes, &b))
          return false;
        ++ix_copy_bytes;
        if (!output->Write(&b, 1))
          return false;
        current_rva += 1;
        break;
      }

      case REL32: {
        uint32 index;
        if (!VectorAt(rel32_ix_, ix_rel32_ix, &index))
          return false;
        ++ix_rel32_ix;
        RVA rva;
        if (!VectorAt(rel32_rva_, index, &rva))
          return false;
        uint32 offset = (rva - (current_rva + 4));
        if (!output->Write(&offset, 4))
          return false;
        current_rva += 4;
        break;
      }

      case ABS32: {
        uint32 index;
        if (!VectorAt(abs32_ix_, ix_abs32_ix, &index))
          return false;
        ++ix_abs32_ix;
        RVA rva;
        if (!VectorAt(abs32_rva_, index, &rva))
          return false;
        uint32 abs32 = static_cast<uint32>(rva + image_base_);
        if (!abs32_relocs_.push_back(current_rva) || !output->Write(&abs32, 4))
          return false;
        current_rva += 4;
        break;
      }

      case MAKE_PE_RELOCATION_TABLE: {
        // We can see the base relocation anywhere, but we only have the
        // information to generate it at the very end.  So we divert the bytes
        // we are generating to a temporary stream.
        if (pending_pe_relocation_table)
          return false;  // Can't have two base relocation tables.

        pending_pe_relocation_table = true;
        output = &bytes_following_relocation_table;
        break;
        // There is a potential problem *if* the instruction stream contains
        // some REL32 relocations following the base relocation and in the same
        // section.  We don't know the size of the table, so 'current_rva' will
        // be wrong, causing REL32 offsets to be miscalculated.  This never
        // happens; the base relocation table is usually in a section of its
        // own, a data-only section, and following everything else in the
        // executable except some padding zero bytes.  We could fix this by
        // emitting an ORIGIN after the MAKE_BASE_RELOCATION_TABLE.
      }

      case MAKE_PE64_RELOCATION_TABLE: {
        if (pending_pe_relocation_table)
          return false;  // Can't have two base relocation tables.

        pending_pe_relocation_table = true;
        pending_pe_relocation_table_type = 0x0A;  // IMAGE_REL_BASED_DIR64
        output = &bytes_following_relocation_table;
        break;
      }

      case MAKE_ELF_ARM_RELOCATION_TABLE: {
        // We can see the base relocation anywhere, but we only have the
        // information to generate it at the very end.  So we divert the bytes
        // we are generating to a temporary stream.
        if (pending_elf_relocation_table_type)
          return false;  // Can't have two base relocation tables.

        pending_elf_relocation_table_type = R_ARM_RELATIVE;
        output = &bytes_following_relocation_table;
        break;
      }

      case MAKE_ELF_RELOCATION_TABLE: {
        // We can see the base relocation anywhere, but we only have the
        // information to generate it at the very end.  So we divert the bytes
        // we are generating to a temporary stream.
        if (pending_elf_relocation_table_type)
          return false;  // Can't have two base relocation tables.

        pending_elf_relocation_table_type = R_386_RELATIVE;
        output = &bytes_following_relocation_table;
        break;
      }
    }
  }

  if (pending_pe_relocation_table) {
    if (!GeneratePeRelocations(final_buffer,
                               pending_pe_relocation_table_type) ||
        !final_buffer->Append(&bytes_following_relocation_table))
      return false;
  }

  if (pending_elf_relocation_table_type) {
    if (!GenerateElfRelocations(pending_elf_relocation_table_type,
                                final_buffer) ||
        !final_buffer->Append(&bytes_following_relocation_table))
      return false;
  }

  // Final verification check: did we consume all lists?
  if (ix_copy_counts != copy_counts_.size())
    return false;
  if (ix_copy_bytes != copy_bytes_.size())
    return false;
  if (ix_abs32_ix != abs32_ix_.size())
    return false;
  if (ix_rel32_ix != rel32_ix_.size())
    return false;

  return true;
}

// RelocBlock has the layout of a block of relocations in the base relocation
// table file format.
//
struct RelocBlockPOD {
  uint32 page_rva;
  uint32 block_size;
  uint16 relocs[4096];  // Allow up to one relocation per byte of a 4k page.
};

COMPILE_ASSERT(offsetof(RelocBlockPOD, relocs) == 8, reloc_block_header_size);

class RelocBlock {
 public:
  RelocBlock() {
    pod.page_rva = 0xFFFFFFFF;
    pod.block_size = 8;
  }

  void Add(uint16 item) {
    pod.relocs[(pod.block_size-8)/2] = item;
    pod.block_size += 2;
  }

  CheckBool Flush(SinkStream* buffer) WARN_UNUSED_RESULT {
    bool ok = true;
    if (pod.block_size != 8) {
      if (pod.block_size % 4 != 0) {  // Pad to make size multiple of 4 bytes.
        Add(0);
      }
      ok = buffer->Write(&pod, pod.block_size);
      pod.block_size = 8;
    }
    return ok;
  }
  RelocBlockPOD pod;
};

CheckBool EncodedProgram::GeneratePeRelocations(SinkStream* buffer,
                                                uint8 type) {
  std::sort(abs32_relocs_.begin(), abs32_relocs_.end());

  RelocBlock block;

  bool ok = true;
  for (size_t i = 0;  ok && i < abs32_relocs_.size();  ++i) {
    uint32 rva = abs32_relocs_[i];
    uint32 page_rva = rva & ~0xFFF;
    if (page_rva != block.pod.page_rva) {
      ok &= block.Flush(buffer);
      block.pod.page_rva = page_rva;
    }
    if (ok)
      block.Add(((static_cast<uint16>(type)) << 12 ) | (rva & 0xFFF));
  }
  ok &= block.Flush(buffer);
  return ok;
}

CheckBool EncodedProgram::GenerateElfRelocations(Elf32_Word r_info,
                                                 SinkStream* buffer) {
  std::sort(abs32_relocs_.begin(), abs32_relocs_.end());

  Elf32_Rel relocation_block;

  relocation_block.r_info = r_info;

  bool ok = true;
  for (size_t i = 0;  ok && i < abs32_relocs_.size();  ++i) {
    relocation_block.r_offset = abs32_relocs_[i];
    ok = buffer->Write(&relocation_block, sizeof(Elf32_Rel));
  }

  return ok;
}
////////////////////////////////////////////////////////////////////////////////

Status WriteEncodedProgram(EncodedProgram* encoded, SinkStreamSet* sink) {
  if (!encoded->WriteTo(sink))
    return C_STREAM_ERROR;
  return C_OK;
}

Status ReadEncodedProgram(SourceStreamSet* streams, EncodedProgram** output) {
  EncodedProgram* encoded = new EncodedProgram();
  if (encoded->ReadFrom(streams)) {
    *output = encoded;
    return C_OK;
  }
  delete encoded;
  return C_DESERIALIZATION_FAILED;
}

Status Assemble(EncodedProgram* encoded, SinkStream* buffer) {
  bool assembled = encoded->AssembleTo(buffer);
  if (assembled)
    return C_OK;
  return C_ASSEMBLY_FAILED;
}

void DeleteEncodedProgram(EncodedProgram* encoded) {
  delete encoded;
}

}  // end namespace
