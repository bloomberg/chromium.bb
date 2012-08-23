// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_CPU_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_CPU_INFO_PROVIDER_H_

#include "chrome/browser/extensions/system_info_provider.h"
#include "chrome/common/extensions/api/experimental_system_info_cpu.h"

namespace extensions {

typedef SystemInfoProvider<api::experimental_system_info_cpu::CpuInfo>
    CpuInfoProvider;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_CPU_INFO_PROVIDER_H_

