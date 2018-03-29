// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_DISASSEMBLER_DEX_H_
#define COMPONENTS_ZUCCHINI_DISASSEMBLER_DEX_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/zucchini/disassembler.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/type_dex.h"

namespace zucchini {

// For consistency, let "canonical order" of DEX data types be the order defined
// in https://source.android.com/devices/tech/dalvik/dex-format "Type Codes"
// section.

class DisassemblerDex : public Disassembler {
 public:
  DisassemblerDex();
  ~DisassemblerDex() override;

  // Applies quick checks to determine if |image| *may* point to the start of an
  // executable. Returns true on success.
  static bool QuickDetect(ConstBufferView image);

  // Disassembler:
  ExecutableType GetExeType() const override;
  std::string GetExeTypeString() const override;
  std::vector<ReferenceGroup> MakeReferenceGroups() const override;

 private:
  friend Disassembler;
  using MapItemMap = std::map<uint16_t, const dex::MapItem*>;

  // Disassembler:
  bool Parse(ConstBufferView image) override;

  bool ParseHeader();

  const dex::HeaderItem* header_ = nullptr;
  int dex_version_ = 0;
  MapItemMap map_item_map_ = {};
  dex::MapItem string_map_item_ = {};
  dex::MapItem type_map_item_ = {};
  dex::MapItem field_map_item_ = {};
  dex::MapItem method_map_item_ = {};
  dex::MapItem code_map_item_ = {};

  // Sorted list of offsets of code items in |image_|.
  std::vector<offset_t> code_item_offsets_;

  DISALLOW_COPY_AND_ASSIGN(DisassemblerDex);
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_DISASSEMBLER_DEX_H_
