// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MEMORY_DUMP_PROVIDER_H_
#define BASE_TRACE_EVENT_MEMORY_DUMP_PROVIDER_H_

#include "base/base_export.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/memory_allocator_attributes_type_info.h"

namespace base {

class SingleThreadTaskRunner;

namespace trace_event {

class ProcessMemoryDump;

// The contract interface that memory dump providers must implement.
class BASE_EXPORT MemoryDumpProvider {
 public:
  // Called by the MemoryDumpManager when generating memory dumps.
  // Returns: true if the |pmd| was successfully populated, false otherwise.
  virtual bool DumpInto(ProcessMemoryDump* pmd) = 0;

  virtual const char* GetFriendlyName() const = 0;

  const MemoryAllocatorAttributesTypeInfo& allocator_attributes_type_info()
      const {
    return allocator_attributes_type_info_;
  }

  // The dump provider can specify an optional thread affinity (in its
  // base constructor call). If |task_runner| is non empty, all the calls to
  // DumpInto are guaranteed to be posted to that TaskRunner.
  const scoped_refptr<SingleThreadTaskRunner>& task_runner() const {
    return task_runner_;
  }

 protected:
  // Default ctor: the MDP is not bound to any thread (must be a singleton).
  MemoryDumpProvider();

  // Use this ctor to ensure that DumpInto() is called always on the same thread
  // specified by |task_runner|.
  explicit MemoryDumpProvider(
      const scoped_refptr<SingleThreadTaskRunner>& task_runner);

  virtual ~MemoryDumpProvider();

  void DeclareAllocatorAttribute(const std::string& allocator_name,
                                 const std::string& attribute_name,
                                 const std::string& attribute_type);

 private:
  // A map of attributes types (declared through DeclareAllocatorAttribute())
  // emitted by this allocator dumper.
  MemoryAllocatorAttributesTypeInfo allocator_attributes_type_info_;

  // (Optional) TaskRunner on which the DumpInfo call should be posted.
  scoped_refptr<SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MemoryDumpProvider);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_MEMORY_DUMP_PROVIDER_H_
