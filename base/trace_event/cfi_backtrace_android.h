// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_CFI_BACKTRACE_ANDROID_H_
#define BASE_TRACE_EVENT_CFI_BACKTRACE_ANDROID_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/base_export.h"
#include "base/debug/debugging_buildflags.h"
#include "base/files/memory_mapped_file.h"
#include "base/gtest_prod_util.h"

namespace base {
namespace trace_event {

// This class is used to unwind stack frames in the current thread. The unwind
// information (dwarf debug info) is stripped from the chrome binary and we do
// not build with exception tables (ARM EHABI) in release builds. So, we use a
// custom unwind table which is generated and added to specific android builds,
// when add_unwind_tables_in_apk build option is specified. This unwind table
// contains information for unwinding stack frames when the functions calls are
// from lib[mono]chrome.so. The file is added as an asset to the apk and the
// table is used to unwind stack frames for profiling. This class implements
// methods to read and parse the unwind table and unwind stack frames using this
// data.
class BASE_EXPORT CFIBacktraceAndroid {
 public:
  // Creates and initializes the unwinder on first call.
  static CFIBacktraceAndroid* GetInstance();

  // Returns true if stack unwinding is possible using CFI unwind tables in apk.
  // There is no need to check this before each unwind call. Will always return
  // the same value based on CFI tables being present in the binary.
  bool can_unwind_stack_frames() const { return can_unwind_stack_frames_; }

  // Returns the program counters by unwinding stack in the current thread in
  // order of latest call frame first. Unwinding works only if
  // can_unwind_stack_frames() returns true. This function does not allocate
  // memory from heap. For each stack frame, this method searches through the
  // unwind table mapped in memory to find the unwind information for function
  // and walks the stack to find all the return address. This only works until
  // the last function call from the chrome.so. We do not have unwind
  // information to unwind beyond any frame outside of chrome.so. Calls to
  // Unwind() are thread safe and lock free, once Initialize() returns success.
  size_t Unwind(const void** out_trace, size_t max_depth) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(CFIBacktraceAndroidTest, TestFindCFIRow);
  FRIEND_TEST_ALL_PREFIXES(CFIBacktraceAndroidTest, TestUnwinding);

  // The CFI information that correspond to an instruction.
  struct CFIRow {
    bool operator==(const CFIBacktraceAndroid::CFIRow& o) const {
      return cfa_offset == o.cfa_offset && ra_offset == o.ra_offset;
    }

    // The offset of the call frame address of previous function from the
    // current stack pointer. Rule for unwinding SP: SP_prev = SP_cur +
    // cfa_offset.
    size_t cfa_offset = 0;
    // The offset of location of return address from the previous call frame
    // address. Rule for unwinding PC: PC_prev = * (SP_prev - ra_offset).
    size_t ra_offset = 0;
  };

  CFIBacktraceAndroid();
  ~CFIBacktraceAndroid();

  // Initializes unwind tables using the CFI asset file in the apk if present.
  // Also stores the limits of mapped region of the lib[mono]chrome.so binary,
  // since the unwind is only feasible for addresses within the .so file. Once
  // initialized, the memory map of the unwind table is never cleared since we
  // cannot guarantee that all the threads are done using the memory map when
  // heap profiling is turned off. But since we keep the memory map is clean,
  // the system can choose to evict the unused pages when needed. This would
  // still reduce the total amount of address space available in process.
  void Initialize();

  // Finds the CFI row for the given |func_addr| in terms of offset from
  // the start of the current binary.
  bool FindCFIRowForPC(uintptr_t func_addr, CFIRow* out) const;

  // Details about the memory mapped region which contains the libchrome.so
  // library file.
  uintptr_t executable_start_addr_ = 0;
  uintptr_t executable_end_addr_ = 0;

  // The start address of the memory mapped unwind table asset file. Unique ptr
  // because it is replaced in tests.
  std::unique_ptr<MemoryMappedFile> cfi_mmap_;
  size_t unwind_table_row_count_ = 0;
  bool can_unwind_stack_frames_ = false;
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_CFI_BACKTRACE_ANDROID_H_
