// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/disassembler.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"

#include "courgette/assembly_program.h"
#include "courgette/courgette.h"
#include "courgette/disassembler_elf_32_arm.h"
#include "courgette/disassembler_elf_32_x86.h"
#include "courgette/disassembler_win32_x64.h"
#include "courgette/disassembler_win32_x86.h"
#include "courgette/encoded_program.h"

namespace courgette {

////////////////////////////////////////////////////////////////////////////////

Disassembler* DetectDisassembler(const void* buffer, size_t length) {
  Disassembler* disassembler = NULL;

  disassembler = new DisassemblerWin32X86(buffer, length);
  if (disassembler->ParseHeader())
    return disassembler;
  else
    delete disassembler;

  disassembler = new DisassemblerWin32X64(buffer, length);
  if (disassembler->ParseHeader())
    return disassembler;
  else
    delete disassembler;

  disassembler = new DisassemblerElf32X86(buffer, length);
  if (disassembler->ParseHeader())
    return disassembler;
  else
    delete disassembler;

  disassembler = new DisassemblerElf32ARM(buffer, length);
  if (disassembler->ParseHeader())
    return disassembler;
  else
    delete disassembler;

  return NULL;
}

Status DetectExecutableType(const void* buffer, size_t length,
                            ExecutableType* type,
                            size_t* detected_length) {

  Disassembler* disassembler = DetectDisassembler(buffer, length);

  if (disassembler) {
    *type = disassembler->kind();
    *detected_length = disassembler->length();
    delete disassembler;
    return C_OK;
  }

  // We failed to detect anything
  *type = EXE_UNKNOWN;
  *detected_length = 0;
  return C_INPUT_NOT_RECOGNIZED;
}

Status ParseDetectedExecutable(const void* buffer, size_t length,
                               AssemblyProgram** output) {
  *output = NULL;

  Disassembler* disassembler = DetectDisassembler(buffer, length);

  if (!disassembler) {
    return C_INPUT_NOT_RECOGNIZED;
  }

  AssemblyProgram* program = new AssemblyProgram(disassembler->kind());

  if (!disassembler->Disassemble(program)) {
    delete program;
    delete disassembler;
    return C_DISASSEMBLY_FAILED;
  }

  delete disassembler;
  *output = program;
  return C_OK;
}

void DeleteAssemblyProgram(AssemblyProgram* program) {
  delete program;
}

Disassembler::Disassembler(const void* start, size_t length)
  : failure_reason_("uninitialized") {

  start_ = reinterpret_cast<const uint8*>(start);
  length_ = length;
  end_ = start_ + length_;
};

Disassembler::~Disassembler() {};

const uint8* Disassembler::OffsetToPointer(size_t offset) const {
  assert(start_ + offset <= end_);
  return start_ + offset;
}

bool Disassembler::Good() {
  failure_reason_ = NULL;
  return true;
}

bool Disassembler::Bad(const char* reason) {
  failure_reason_ = reason;
  return false;
}

void Disassembler::ReduceLength(size_t reduced_length) {
  if (reduced_length < length_)
    length_ = reduced_length;
}

}  // namespace courgette
