// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_cpu/cpu_info_provider.h"

namespace extensions {

bool CpuInfoProvider::QueryCpuTimePerProcessor(
    std::vector<linked_ptr<api::system_cpu::ProcessorInfo> >* infos) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace extensions
