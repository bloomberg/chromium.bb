// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MALLOC_DUMP_PROVIDER_H_
#define BASE_TRACE_EVENT_MALLOC_DUMP_PROVIDER_H_

#include <istream>

#include "base/memory/singleton.h"
#include "base/trace_event/memory_dump_provider.h"

namespace base {
namespace trace_event {

// Dump provider which collects process-wide memory stats.
class BASE_EXPORT MallocDumpProvider : public MemoryDumpProvider {
 public:
  static MallocDumpProvider* GetInstance();

  // MemoryDumpProvider implementation.
  bool DumpInto(ProcessMemoryDump* pmd) override;
  const char* GetFriendlyName() const override;

 private:
  friend struct DefaultSingletonTraits<MallocDumpProvider>;

  MallocDumpProvider();
  ~MallocDumpProvider() override;

  DISALLOW_COPY_AND_ASSIGN(MallocDumpProvider);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_MALLOC_DUMP_PROVIDER_H_
