// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/zucchini_tools.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <ostream>
#include <set>

#include "base/strings/stringprintf.h"
#include "chrome/installer/zucchini/disassembler.h"
#include "chrome/installer/zucchini/element_detection.h"
#include "chrome/installer/zucchini/io_utils.h"

namespace zucchini {

status::Code ReadReferences(ConstBufferView image,
                            bool do_dump,
                            std::ostream& out) {
  std::unique_ptr<Disassembler> disasm = MakeDisassemblerWithoutFallback(image);
  if (!disasm) {
    out << "Input file not recognized as executable." << std::endl;
    return status::kStatusInvalidOldImage;
  }

  auto get_num_locations = [](const ReferenceGroup& group,
                              Disassembler* disasm) {
    size_t count = 0;
    auto refs = group.GetReader(disasm);
    for (auto ref = refs->GetNext(); ref.has_value(); ref = refs->GetNext())
      ++count;
    return count;
  };

  auto get_num_targets = [](const ReferenceGroup& group, Disassembler* disasm) {
    std::set<offset_t> target_set;
    auto refs = group.GetReader(disasm);
    for (auto ref = refs->GetNext(); ref.has_value(); ref = refs->GetNext())
      target_set.insert(ref->target);
    return target_set.size();
  };

  for (const auto& group : disasm->MakeReferenceGroups()) {
    out << "Type " << int(group.type_tag().value());
    out << ": Pool=" << static_cast<uint32_t>(group.pool_tag().value());
    out << ", width=" << group.width();
    size_t num_locations = get_num_locations(group, disasm.get());
    out << ", #locations=" << num_locations;
    size_t num_targets = get_num_targets(group, disasm.get());
    out << ", #targets=" << num_targets;
    if (num_targets > 0) {
      double ratio = static_cast<double>(num_locations) / num_targets;
      out << " (ratio=" << base::StringPrintf("%.4f", ratio) << ")";
    }
    out << std::endl;

    if (do_dump) {
      auto refs = group.GetReader(disasm.get());

      for (auto ref = refs->GetNext(); ref; ref = refs->GetNext()) {
        out << "  " << AsHex<8>(ref->location);
        out << " " << AsHex<8>(ref->target) << std::endl;
      }
    }
  }

  return status::kStatusSuccess;
}

}  // namespace zucchini
