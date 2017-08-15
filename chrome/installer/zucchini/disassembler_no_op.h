// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_DISASSEMBLER_NO_OP_H_
#define CHROME_INSTALLER_ZUCCHINI_DISASSEMBLER_NO_OP_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/installer/zucchini/buffer_view.h"
#include "chrome/installer/zucchini/disassembler.h"
#include "chrome/installer/zucchini/image_utils.h"

namespace zucchini {

// This disassembler works on any file and does not look for reference.
class DisassemblerNoOp : public Disassembler {
 public:
  static std::unique_ptr<DisassemblerNoOp> Make(ConstBufferView image);

  ~DisassemblerNoOp() override;

  // Disassembler:
  ExecutableType GetExeType() const override;
  std::string GetExeTypeString() const override;
  std::vector<ReferenceGroup> MakeReferenceGroups() const override;

 protected:
  DisassemblerNoOp();

 private:
  bool Parse(ConstBufferView image) override;

  DISALLOW_COPY_AND_ASSIGN(DisassemblerNoOp);
};

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_DISASSEMBLER_NO_OP_H_
