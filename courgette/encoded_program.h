// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_ENCODED_PROGRAM_H_
#define COURGETTE_ENCODED_PROGRAM_H_

#include <vector>

#include "base/basictypes.h"
#include "courgette/image_info.h"
#include "courgette/memory_allocator.h"

namespace courgette {

class SinkStream;
class SinkStreamSet;
class SourceStreamSet;

// An EncodedProgram is a set of tables that contain a simple 'binary assembly
// language' that can be assembled to produce a sequence of bytes, for example,
// a Windows 32-bit executable.
//
class EncodedProgram {
 public:
  EncodedProgram();
  ~EncodedProgram();

  // Generating an EncodedProgram:
  //
  // (1) The image base can be specified at any time.
  void set_image_base(uint64 base) { image_base_ = base; }

  // (2) Address tables and indexes defined first.
  void DefineRel32Label(int index, RVA address);
  void DefineAbs32Label(int index, RVA address);
  void EndLabels();

  // (3) Add instructions in the order needed to generate bytes of file.
  void AddOrigin(RVA rva);
  void AddCopy(uint32 count, const void* bytes);
  void AddRel32(int label_index);
  void AddAbs32(int label_index);
  void AddMakeRelocs();

  // (3) Serialize binary assembly language tables to a set of streams.
  void WriteTo(SinkStreamSet *streams);

  // Using an EncodedProgram to generate a byte stream:
  //
  // (4) Deserializes a fresh EncodedProgram from a set of streams.
  bool ReadFrom(SourceStreamSet *streams);

  // (5) Assembles the 'binary assembly language' into final file.
  bool AssembleTo(SinkStream *buffer);

 private:
  // Binary assembly language operations.
  enum OP {
    ORIGIN,    // ORIGIN <rva> - set address for subsequent assembly.
    COPY,      // COPY <count> <bytes> - copy bytes to output.
    COPY1,     // COPY1 <byte> - same as COPY 1 <byte>.
    REL32,     // REL32 <index> - emit rel32 encoded reference to address at
               // address table offset <index>
    ABS32,     // ABS32 <index> - emit abs32 encoded reference to address at
               // address table offset <index>
    MAKE_BASE_RELOCATION_TABLE,  // Emit base relocation table blocks.
    OP_LAST
  };

  typedef std::vector<RVA, MemoryAllocator<RVA> > RvaVector;
  typedef std::vector<uint32, MemoryAllocator<uint32> > UInt32Vector;
  typedef std::vector<uint8, MemoryAllocator<uint8> > UInt8Vector;
  typedef std::vector<OP, MemoryAllocator<OP> > OPVector;

  void DebuggingSummary();
  void GenerateBaseRelocations(SinkStream *buffer);
  void DefineLabelCommon(RvaVector*, int, RVA);
  void FinishLabelsCommon(RvaVector* addresses);

  // Binary assembly language tables.
  uint64 image_base_;
  RvaVector rel32_rva_;
  RvaVector abs32_rva_;
  OPVector ops_;
  RvaVector origins_;
  UInt32Vector copy_counts_;
  UInt8Vector copy_bytes_;
  UInt32Vector rel32_ix_;
  UInt32Vector abs32_ix_;

  // Table of the addresses containing abs32 relocations; computed during
  // assembly, used to generate base relocation table.
  UInt32Vector abs32_relocs_;

  DISALLOW_COPY_AND_ASSIGN(EncodedProgram);
};

}  // namespace courgette
#endif  // COURGETTE_ENCODED_PROGRAM_H_
