// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_CPU_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_CPU_INFO_PROVIDER_H_

#include "chrome/common/extensions/api/experimental_system_info_cpu.h"

namespace extensions {

// An interface for retrieving cpu information on different platforms.
class CpuInfoProvider {
 public:
   // Return a CpuInfoProvider instance. The caller is responsible for
   // releasing it.
   static CpuInfoProvider* Create();

   virtual ~CpuInfoProvider() {}

   // Return true if succeed to get CPU information, otherwise return false.
   // Should be implemented on different platforms.
   virtual bool GetCpuInfo(
       api::experimental_system_info_cpu::CpuInfo* info) = 0;
};

}  // namespace extensions
#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_CPU_INFO_PROVIDER_H_
