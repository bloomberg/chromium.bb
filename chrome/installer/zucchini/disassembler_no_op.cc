// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/disassembler_no_op.h"

namespace zucchini {

DisassemblerNoOp::DisassemblerNoOp() = default;
DisassemblerNoOp::~DisassemblerNoOp() = default;

ExecutableType DisassemblerNoOp::GetExeType() const {
  return kExeTypeNoOp;
}

std::string DisassemblerNoOp::GetExeTypeString() const {
  return "(Unknown)";
}

std::vector<ReferenceGroup> DisassemblerNoOp::MakeReferenceGroups() const {
  return std::vector<ReferenceGroup>();
}

bool DisassemblerNoOp::Parse(ConstBufferView image) {
  return true;
}

}  // namespace zucchini
