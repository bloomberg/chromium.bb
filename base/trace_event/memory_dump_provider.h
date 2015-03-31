// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MEMORY_DUMP_PROVIDER_H_
#define BASE_TRACE_EVENT_MEMORY_DUMP_PROVIDER_H_

#include "base/base_export.h"
#include "base/trace_event/memory_allocator_attributes.h"

namespace base {
namespace trace_event {

class ProcessMemoryDump;

// The contract interface that memory dump providers must implement.
class BASE_EXPORT MemoryDumpProvider {
 public:
  // Called by the MemoryDumpManager when generating memory dumps.
  // Returns: true if the |pmd| was successfully populated, false otherwise.
  virtual bool DumpInto(ProcessMemoryDump* pmd) = 0;

  virtual const char* GetFriendlyName() const = 0;

  const MemoryAllocatorDeclaredAttributes& allocator_attributes() const {
    return allocator_attributes_;
  }

 protected:
  MemoryDumpProvider();
  virtual ~MemoryDumpProvider();

  void DeclareAllocatorAttribute(const MemoryAllocatorDeclaredAttribute& attr);

 private:
  // The map (attribute name -> type) that specifies the semantic of the
  // extra attributes that the MemoryAllocatorDump(s) produced by this
  // MemoryDumpProvider will have.
  MemoryAllocatorDeclaredAttributes allocator_attributes_;

  DISALLOW_COPY_AND_ASSIGN(MemoryDumpProvider);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_MEMORY_DUMP_PROVIDER_H_
