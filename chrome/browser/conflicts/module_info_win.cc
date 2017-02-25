// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_info_win.h"

// ModuleInfoKey ---------------------------------------------------------------

ModuleInfoKey::ModuleInfoKey(const base::FilePath& module_path,
                             uint32_t module_size,
                             uint32_t module_time_date_stamp,
                             uint32_t module_id)
    : module_path(module_path),
      module_size(module_size),
      module_time_date_stamp(module_time_date_stamp),
      module_id(module_id) {}

bool ModuleInfoKey::operator<(const ModuleInfoKey& mik) const {
  // The key consists of the triplet of
  // (module_path, module_size, module_time_date_stamp).
  // Use the std::tuple lexicographic comparison operator.
  return std::make_tuple(module_path, module_size, module_time_date_stamp) <
         std::make_tuple(mik.module_path, mik.module_size,
                         mik.module_time_date_stamp);
}

// ModuleInfoData --------------------------------------------------------------

ModuleInfoData::ModuleInfoData() : process_types(0) {}

ModuleInfoData::ModuleInfoData(const ModuleInfoData& others) = default;

ModuleInfoData::~ModuleInfoData() = default;
