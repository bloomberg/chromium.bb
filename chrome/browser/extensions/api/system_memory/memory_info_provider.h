// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_MEMORY_MEMORY_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_MEMORY_MEMORY_INFO_PROVIDER_H_

#include "chrome/browser/extensions/api/system_info/system_info_provider.h"
#include "chrome/common/extensions/api/system_memory.h"

namespace extensions {

class MemoryInfoProvider
    : public SystemInfoProvider<api::system_memory::MemoryInfo> {
 public:
  static MemoryInfoProvider* Get();

  // Overriden from SystemInfoProvider<MemoryInfo>.
  virtual bool QueryInfo() OVERRIDE;

  const api::system_memory::MemoryInfo& memory_info() const;

 private:
  friend class SystemInfoProvider<api::system_memory::MemoryInfo>;
  friend class MockMemoryInfoProviderImpl;

  MemoryInfoProvider();
  virtual ~MemoryInfoProvider();

  DISALLOW_COPY_AND_ASSIGN(MemoryInfoProvider);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_MEMORY_MEMORY_INFO_PROVIDER_H_

