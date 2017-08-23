// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/element_detection.h"

#include <utility>

#include "base/logging.h"
#include "chrome/installer/zucchini/disassembler.h"
#include "chrome/installer/zucchini/disassembler_no_op.h"
#include "chrome/installer/zucchini/disassembler_win32.h"

namespace zucchini {

namespace {

// Impose a minimal program size to eliminate pathological cases.
constexpr size_t kMinProgramSize = 16;

}  // namespace

/******** Utility Functions ********/

std::unique_ptr<Disassembler> MakeDisassemblerWithoutFallback(
    ConstBufferView image) {
  if (DisassemblerWin32X86::QuickDetect(image)) {
    auto disasm = DisassemblerWin32X86::Make(image);
    if (disasm && disasm->size() >= kMinProgramSize)
      return disasm;
  }

  if (DisassemblerWin32X64::QuickDetect(image)) {
    auto disasm = DisassemblerWin32X64::Make(image);
    if (disasm && disasm->size() >= kMinProgramSize)
      return disasm;
  }

  return nullptr;
}

std::unique_ptr<Disassembler> MakeDisassemblerOfType(ConstBufferView image,
                                                     ExecutableType exe_type) {
  switch (exe_type) {
    case kExeTypeWin32X86:
      return DisassemblerWin32X86::Make(image);
    case kExeTypeWin32X64:
      return DisassemblerWin32X64::Make(image);
    case kExeTypeNoOp:
      return DisassemblerNoOp::Make(image);
    default:
      return nullptr;
  }
}

std::unique_ptr<Disassembler> MakeNoOpDisassembler(ConstBufferView image) {
  return DisassemblerNoOp::Make(image);
}

base::Optional<Element> DetectElementFromDisassembler(ConstBufferView image) {
  std::unique_ptr<Disassembler> disasm = MakeDisassemblerWithoutFallback(image);
  if (disasm)
    return Element(disasm->GetExeType(), {0, disasm->size()});
  return base::nullopt;
}

/******** ProgramScanner ********/

ElementFinder::ElementFinder(ConstBufferView image, ElementDetector&& detector)
    : image_(image), detector_(std::move(detector)) {}

ElementFinder::~ElementFinder() = default;

base::Optional<Element> ElementFinder::GetNext() {
  for (; pos_ < image_.size(); ++pos_) {
    ConstBufferView test_image =
        ConstBufferView::FromRange(image_.begin() + pos_, image_.end());
    base::Optional<Element> element = detector_.Run(test_image);
    if (element) {
      element->offset += pos_;
      pos_ = element->EndOffset();
      return element;
    }
  }
  return base::nullopt;
}

}  // namespace zucchini
