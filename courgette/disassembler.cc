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
#include "courgette/disassembler_win32_x86.h"
#include "courgette/encoded_program.h"
#include "courgette/image_info.h"

// COURGETTE_HISTOGRAM_TARGETS prints out a histogram of how frequently
// different target addresses are referenced.  Purely for debugging.
#define COURGETTE_HISTOGRAM_TARGETS 0

namespace courgette {

////////////////////////////////////////////////////////////////////////////////

ExecutableType DetectExecutableType(const void* buffer, size_t length) {

  bool parsed = false;

  PEInfo* pe_info = new PEInfo();
  pe_info->Init(buffer, length);
  parsed = pe_info->ParseHeader();
  delete pe_info;

  if (parsed)
    return WIN32_X86;

  return UNKNOWN;
}

Status ParseDetectedExecutable(const void* buffer, size_t length,
                               AssemblyProgram** output) {
  *output = NULL;

  PEInfo* pe_info = new PEInfo();
  pe_info->Init(buffer, length);

  if (!pe_info->ParseHeader()) {
    delete pe_info;
    return C_INPUT_NOT_RECOGNIZED;
  }

  Disassembler* disassembler = new DisassemblerWin32X86(pe_info);
  AssemblyProgram* program = new AssemblyProgram();

  if (!disassembler->Disassemble(program)) {
    delete program;
    delete disassembler;
    delete pe_info;
    return C_DISASSEMBLY_FAILED;
  }

  delete disassembler;
  delete pe_info;
  *output = program;
  return C_OK;
}

void DeleteAssemblyProgram(AssemblyProgram* program) {
  delete program;
}

}  // namespace courgette
