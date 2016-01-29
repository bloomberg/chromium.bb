// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/program_detector.h"

#include <utility>

#include "courgette/assembly_program.h"
#include "courgette/disassembler.h"
#include "courgette/disassembler_elf_32_arm.h"
#include "courgette/disassembler_elf_32_x86.h"
#include "courgette/disassembler_win32_x64.h"
#include "courgette/disassembler_win32_x86.h"

namespace courgette {

namespace {

// Returns a new instance of Disassembler subclass if binary data given in
// |buffer| and |length| matches a known binary format, otherwise null.
scoped_ptr<Disassembler> DetectDisassembler(const void* buffer, size_t length) {
  scoped_ptr<Disassembler> disassembler;

  disassembler.reset(new DisassemblerWin32X86(buffer, length));
  if (disassembler->ParseHeader())
    return disassembler;

  disassembler.reset(new DisassemblerWin32X64(buffer, length));
  if (disassembler->ParseHeader())
    return disassembler;

  disassembler.reset(new DisassemblerElf32X86(buffer, length));
  if (disassembler->ParseHeader())
    return disassembler;

  disassembler.reset(new DisassemblerElf32ARM(buffer, length));
  if (disassembler->ParseHeader())
    return disassembler;

  return nullptr;
}

}  // namespace

Status DetectExecutableType(const void* buffer,
                            size_t length,
                            ExecutableType* type,
                            size_t* detected_length) {
  scoped_ptr<Disassembler> disassembler(DetectDisassembler(buffer, length));

   if (!disassembler) {  // We failed to detect anything.
    *type = EXE_UNKNOWN;
    *detected_length = 0;
    return C_INPUT_NOT_RECOGNIZED;
  }

  *type = disassembler->kind();
  *detected_length = disassembler->length();
  return C_OK;
}

Status ParseDetectedExecutable(const void* buffer,
                               size_t length,
                               scoped_ptr<AssemblyProgram>* output) {
  output->reset();

  scoped_ptr<Disassembler> disassembler(DetectDisassembler(buffer, length));
  if (!disassembler)
    return C_INPUT_NOT_RECOGNIZED;

  scoped_ptr<AssemblyProgram> program(
      new AssemblyProgram(disassembler->kind()));

  if (!disassembler->Disassemble(program.get()))
    return C_DISASSEMBLY_FAILED;

  if (!program->TrimLabels())
    return C_TRIM_FAILED;

  *output = std::move(program);
  return C_OK;
}

}  // namespace courgette
