// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_DISASSEMBLER_H_
#define COURGETTE_DISASSEMBLER_H_

#include "base/basictypes.h"

namespace courgette {

class AssemblyProgram;
class PEInfo;

class Disassembler {
 public:
  virtual ~Disassembler() {}

  // Disassembles the item passed to the factory method into the output
  // parameter 'program'.
  virtual bool Disassemble(AssemblyProgram* program) = 0;

 protected:
  Disassembler() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Disassembler);
};

}  // namespace courgette
#endif  // COURGETTE_DISASSEMBLER_H_
