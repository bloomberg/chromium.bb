// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CRASH_HANDLER_CRASH_ANALYZER_H_
#define COMPONENTS_GWP_ASAN_CRASH_HANDLER_CRASH_ANALYZER_H_

#include <stddef.h>

#include "components/gwp_asan/common/allocator_state.h"
#include "components/gwp_asan/crash_handler/crash.pb.h"
#include "third_party/crashpad/crashpad/util/misc/address_types.h"

namespace crashpad {
class ExceptionSnapshot;
class ProcessMemory;
class ProcessSnapshot;
}  // namespace crashpad

namespace gwp_asan {
namespace internal {

class CrashAnalyzer {
 public:
  // Captures the result of the GWP-ASan crash analyzer, whether the crash is
  // determined to be related or unrelated to GWP-ASan or if an error was
  // encountered analyzing the exception.
  //
  // These values are persisted via UMA--entries should not be renumbered and
  // numeric values should never be reused.
  enum class GwpAsanCrashAnalysisResult {
    // The crash is not caused by GWP-ASan.
    kUnrelatedCrash = 0,
    // The crash is caused by GWP-ASan.
    kGwpAsanCrash = 1,
    // The ProcessMemory from the snapshot was null.
    kErrorNullProcessMemory = 2,
    // Failed to read the crashing process' memory of the global allocator.
    kErrorFailedToReadAllocator = 3,
    // The crashing process' global allocator members failed sanity checks.
    kErrorAllocatorFailedSanityCheck = 4,
    // Failed to read crash stack traces.
    kErrorFailedToReadStackTrace = 5,
    // The ExceptionSnapshot CPU context was null.
    kErrorNullCpuContext = 6,
    // The crashing process' bitness does not match the crash handler.
    kErrorMismatchedBitness = 7,
    // The allocator computed an invalid slot index.
    kErrorBadSlot = 8,
    // Number of values in this enumeration, required by UMA.
    kMaxValue = kErrorBadSlot
  };

  // Given a ProcessSnapshot, determine if the exception is related to GWP-ASan.
  // If it is, return kGwpAsanCrash and fill out the info parameter with
  // details about the exception. Otherwise, return a value indicating that the
  // crash is unrelated or that an error occured.
  static GwpAsanCrashAnalysisResult GetExceptionInfo(
      const crashpad::ProcessSnapshot& process_snapshot,
      gwp_asan::Crash* proto);

 private:
  using SlotMetadata = AllocatorState::SlotMetadata;

  // The maximum stack trace size the analyzer will extract from a crashing
  // process.
  static constexpr size_t kMaxStackTraceLen = 64;

  // Given an ExceptionSnapshot, return the address of where the exception
  // occurred (or null if it was not a data access exception.)
  static crashpad::VMAddress GetAccessAddress(
      const crashpad::ExceptionSnapshot& exception);

  // If the allocator annotation is present in the given snapshot, then return
  // the address for the GuardedPageAllocator in the crashing process.
  static crashpad::VMAddress GetAllocatorAddress(
      const crashpad::ProcessSnapshot& process_snapshot);

  // This method implements the underlying logic for GetExceptionInfo(). It
  // analyzes the GuardedPageAllocator of the crashing process, and if the
  // exception address is in the GWP-ASan region it fills out the protobuf
  // parameter and returns kGwpAsanCrash.
  static GwpAsanCrashAnalysisResult AnalyzeCrashedAllocator(
      const crashpad::ProcessMemory& memory,
      crashpad::VMAddress gpa_addr,
      crashpad::VMAddress exception_addr,
      gwp_asan::Crash* proto);

  // This method fills out an AllocationInfo protobuf from a
  // SlotMetadata::AllocationInfo struct.
  static bool ReadAllocationInfo(const crashpad::ProcessMemory& memory,
                                 const SlotMetadata::AllocationInfo& slot_info,
                                 gwp_asan::Crash_AllocationInfo* proto_info);

  // Read a stack trace from a crashing process with address crashing_trace_addr
  // and length trace_len into trace_output. On failure returns false.
  static bool ReadStackTrace(const crashpad::ProcessMemory& memory,
                             crashpad::VMAddress crashing_trace_addr,
                             uintptr_t* trace_output,
                             size_t trace_len);
};

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_CRASH_HANDLER_CRASH_ANALYZER_H_
