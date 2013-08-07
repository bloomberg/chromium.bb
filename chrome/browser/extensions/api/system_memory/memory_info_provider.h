// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_MEMORY_MEMORY_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_MEMORY_MEMORY_INFO_PROVIDER_H_

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/system_info/system_info_provider.h"
#include "chrome/common/extensions/api/system_memory.h"

namespace extensions {

class MemoryInfoProvider : public SystemInfoProvider {
 public:
  static MemoryInfoProvider* Get();

  const api::system_memory::MemoryInfo& memory_info() const;

  static void InitializeForTesting(scoped_refptr<MemoryInfoProvider> provider);

 private:
  friend class MockMemoryInfoProviderImpl;

  MemoryInfoProvider();
  virtual ~MemoryInfoProvider();

  // Overriden from SystemInfoProvider.
  virtual bool QueryInfo() OVERRIDE;

  // The last information filled up by QueryInfo and is accessed on multiple
  // threads, but the whole class is being guarded by SystemInfoProvider base
  // class.
  //
  // |info_| is accessed on the UI thread while |is_waiting_for_completion_| is
  // false and on the sequenced worker pool while |is_waiting_for_completion_|
  // is true.
  api::system_memory::MemoryInfo info_;

  static base::LazyInstance<scoped_refptr<MemoryInfoProvider> > provider_;

  DISALLOW_COPY_AND_ASSIGN(MemoryInfoProvider);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_MEMORY_MEMORY_INFO_PROVIDER_H_
