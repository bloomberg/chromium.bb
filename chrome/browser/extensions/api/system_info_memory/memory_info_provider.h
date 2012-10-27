// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_MEMORY_MEMORY_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_MEMORY_MEMORY_INFO_PROVIDER_H_

#include "chrome/browser/extensions/system_info_provider.h"
#include "chrome/common/extensions/api/experimental_system_info_memory.h"

namespace extensions {

class MemoryInfoProvider
    : public SystemInfoProvider<
          api::experimental_system_info_memory::MemoryInfo> {
 public:
  static MemoryInfoProvider* Get();

  // Overriden from SystemInfoProvider<MemoryInfo>.
  virtual bool QueryInfo(
      api::experimental_system_info_memory::MemoryInfo* info) OVERRIDE;

 private:
  friend class SystemInfoProvider<
      api::experimental_system_info_memory::MemoryInfo>;
  friend class MockMemoryInfoProviderImpl;

  MemoryInfoProvider();
  virtual ~MemoryInfoProvider();

  DISALLOW_COPY_AND_ASSIGN(MemoryInfoProvider);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_MEMORY_MEMORY_INFO_PROVIDER_H_

