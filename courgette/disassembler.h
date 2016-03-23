// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_DISASSEMBLER_H_
#define COURGETTE_DISASSEMBLER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "courgette/courgette.h"
#include "courgette/image_utils.h"

namespace courgette {

class AssemblyProgram;

class Disassembler : public AddressTranslator {
 public:
  virtual ~Disassembler();

  // AddressTranslator interfaces.
  virtual RVA FileOffsetToRVA(FileOffset file_offset) const override = 0;
  virtual FileOffset RVAToFileOffset(RVA rva) const override = 0;
  const uint8_t* FileOffsetToPointer(FileOffset file_offset) const override;
  const uint8_t* RVAToPointer(RVA rva) const override;
  RVA PointerToTargetRVA(const uint8_t* p) const = 0;

  virtual ExecutableType kind() const = 0;

  // Returns true if the buffer appears to be a valid executable of the expected
  // type, and false otherwise. This needs not be called before Disassemble().
  virtual bool ParseHeader() = 0;

  // Disassembles the item passed to the factory method into the output
  // parameter 'program'.
  virtual bool Disassemble(AssemblyProgram* program) = 0;

  // ok() may always be called but returns true only after ParseHeader()
  // succeeds.
  bool ok() const { return failure_reason_ == nullptr; }

  // Returns the length of the image. May reduce after ParseHeader().
  size_t length() const { return length_; }
  const uint8_t* start() const { return start_; }
  const uint8_t* end() const { return end_; }

 protected:
  Disassembler(const void* start, size_t length);

  bool Good();
  bool Bad(const char *reason);

  // Returns true if the array lies within our memory region.
  bool IsArrayInBounds(size_t offset, size_t elements, size_t element_size) {
    return offset <= length() && elements <= (length() - offset) / element_size;
  }

  // Reduce the length of the image in memory. Does not actually free
  // (or realloc) any memory. Usually only called via ParseHeader().
  void ReduceLength(size_t reduced_length);

 private:
  const char* failure_reason_;

  //
  // Basic information that is always valid after construction, although
  // ParseHeader() may shorten |length_| if the executable is shorter than the
  // total data.
  //
  size_t length_;         // In current memory.
  const uint8_t* start_;  // In current memory, base for 'file offsets'.
  const uint8_t* end_;    // In current memory.

  DISALLOW_COPY_AND_ASSIGN(Disassembler);
};

}  // namespace courgette

#endif  // COURGETTE_DISASSEMBLER_H_
