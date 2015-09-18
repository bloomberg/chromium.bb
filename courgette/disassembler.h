// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_DISASSEMBLER_H_
#define COURGETTE_DISASSEMBLER_H_

#include "base/basictypes.h"

#include "courgette/courgette.h"
#include "courgette/image_utils.h"

namespace courgette {

class AssemblyProgram;

class Disassembler {
 public:
  virtual ~Disassembler();

  virtual ExecutableType kind() { return EXE_UNKNOWN; }

  // ok() may always be called but returns 'true' only after ParseHeader
  // succeeds.
  bool ok() const { return failure_reason_ == NULL; }

  // Returns 'true' if the buffer appears to be a valid executable of the
  // expected type. It is not required that this be called before Disassemble.
  virtual bool ParseHeader() = 0;

  // Disassembles the item passed to the factory method into the output
  // parameter 'program'.
  virtual bool Disassemble(AssemblyProgram* program) = 0;

  // Returns the length of the source executable. May reduce after ParseHeader.
  size_t length() const { return length_; }
  const uint8* start() const { return start_; }
  const uint8* end() const { return end_; }

  // Returns a pointer into the memory copy of the file format.
  // FileOffsetToPointer(0) returns a pointer to the start of the file format.
  const uint8* OffsetToPointer(size_t offset) const;

 protected:
  Disassembler(const void* start, size_t length);

  bool Good();
  bool Bad(const char *reason);

  // Returns true if the array lies within our memory region.
  bool IsArrayInBounds(size_t offset, size_t elements, size_t element_size) {
    return offset <= length() && elements <= (length() - offset) / element_size;
  }

  // Reduce the length of the image in memory. Does not actually free
  // (or realloc) any memory. Usually only called via ParseHeader()
  void ReduceLength(size_t reduced_length);

 private:
  const char* failure_reason_;

  //
  // Basic information that is always valid after Construction, though
  // ParseHeader may shorten the length if the executable is shorter than
  // the total data.
  //
  size_t length_;         // In current memory.
  const uint8* start_;    // In current memory, base for 'file offsets'.
  const uint8* end_;      // In current memory.

  DISALLOW_COPY_AND_ASSIGN(Disassembler);
};

}  // namespace courgette

#endif  // COURGETTE_DISASSEMBLER_H_
