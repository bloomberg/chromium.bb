// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/cfi_backtrace_android.h"

#include <sys/mman.h>
#include <sys/types.h>

#include "base/android/apk_assets.h"
#include "base/debug/stack_trace.h"

#if !defined(ARCH_CPU_ARMEL)
#error This file should not be built for this architecture.
#endif

/*
Basics of unwinding:
For each instruction in a function we need to know what is the offset of SP
(Stack Pointer) to reach the previous function's stack frame. To know which
function is being invoked, we need the return address of the next function. The
CFI information for an instruction is made up of 2 offsets, CFA (Call Frame
Address) offset and RA (Return Address) offset. The CFA offset is the change in
SP made by the function till the current instruction. This depends on amount of
memory allocated on stack by the function plus some registers that the function
stores that needs to be restored at the end of function. So, at each instruction
the CFA offset tells the offset from original SP before the function call. The
RA offset tells us the offset from the previous SP into the current function
where the return address is stored.

The unwind table contains rows of 64 bits each.
We have 2 types of rows, FUNCTION and CFI.
Each function with CFI info has a single FUNCTION row, followed by one or more
CFI rows. All the addresses of the CFI rows will be within the function.
1. FUNCTION. Bits in order of high to low represent:
    31 bits: specifies function address, without the last bit (always 0).
     1 bit : always 1. Last bit of the address, specifies the row type is
             FUNCTION.
    32 bits: length of the current function.

2. CFI. Bits in order of high to low represent:
    31 bits: instruction address in the current function.
     1 bit : always 0. Last bit of the address, specifies the row type is CFI.
    30 bits: CFA offset / 4.
     2 bits: RA offset / 4.
If the RA offset of a row is 0, then use the offset of the previous rows in the
same function.
TODO(ssid): Make sure RA offset is always present.

See extract_unwind_tables.py for details about how this data is extracted from
breakpad symbol files.
*/

extern "C" {
extern char __executable_start;
extern char _etext;
}

namespace base {
namespace trace_event {

namespace {

// The bit of the address that is used to specify the type of the row is
// FUNCTION or CFI type.
constexpr uint32_t kFunctionTypeMask = 0x1;

// The mask on the CFI row data that is used to get the high 30 bits and
// multiply it by 4 to get CFA offset. Since the last 2 bits are masked out, a
// shift is not necessary.
constexpr uint32_t kCFAMask = 0xfffffffc;

// The mask on the CFI row data that is used to get the low 2 bits and multiply
// it by 4 to get the RA offset.
constexpr uint32_t kRAMask = 0x3;
constexpr uint32_t kRAShift = 2;

// The code in this file assumes we are running in 32-bit builds since all the
// addresses in the unwind table are specified in 32 bits.
static_assert(sizeof(uintptr_t) == 4,
              "The unwind table format is only valid for 32 bit builds.");

// The struct that corresponds to each row in the unwind table. The row can be
// of any type, CFI or FUNCTION. The first 4 bytes in the row represents the
// address and the next 4 bytes have data. The members of this struct is in
// order of the input format. We cast the memory map of the unwind table as an
// array of CFIUnwindInfo and use it to read data and search. So, the size of
// this struct should be 8 bytes and the order of the members is fixed according
// to the given format.
struct CFIUnwindInfo {
  // The address is either the start address of the function or the instruction
  // address where the CFI information changes in a function. If the last bit of
  // the address is 1 then it specifies that the row is of type FUNCTION and if
  // the last bit is 0 then it specifies the row is of type CFI.
  uintptr_t addr;

  // If the row type is function, |data| is |function_length|. If the row type
  // is CFI, |data| is |cfi_data|.
  union {
    // Represents the total length of the function that start with the |addr|.
    uintptr_t function_length;
    // Represents the CFA and RA offsets to get information about next stack
    // frame.
    uintptr_t cfi_data;
  } data;

  bool is_function_type() const { return !!(addr & kFunctionTypeMask); }

  // Returns the address of the current row, CFI or FUNCTION type.
  uintptr_t address() const {
    return is_function_type() ? (addr & ~kFunctionTypeMask) : addr;
  }

  // Return the RA offset when the current row is CFI type.
  uintptr_t ra_offset() const {
    DCHECK(!is_function_type());
    return (data.cfi_data & kRAMask) << kRAShift;
  }

  // Returns the CFA offset if the current row is CFI type.
  uintptr_t cfa_offset() const {
    DCHECK(!is_function_type());
    return data.cfi_data & kCFAMask;
  }

  // Returns true if the instruction is within the function address range, given
  // that the current row is FUNCTION type and the |instruction_addr| is offset
  // address of instruction from the start of the binary.
  bool is_instruction_in_function(uintptr_t instruction_addr) const {
    DCHECK(is_function_type());
    return (instruction_addr >= address()) &&
           (instruction_addr <= address() + data.function_length);
  }
};

static_assert(
    sizeof(CFIUnwindInfo) == 8,
    "The CFIUnwindInfo struct must be exactly 8 bytes for searching.");

}  // namespace

// static
CFIBacktraceAndroid* CFIBacktraceAndroid::GetInstance() {
  static CFIBacktraceAndroid* instance = new CFIBacktraceAndroid();
  return instance;
}

CFIBacktraceAndroid::CFIBacktraceAndroid() {
  Initialize();
}

CFIBacktraceAndroid::~CFIBacktraceAndroid() {}

void CFIBacktraceAndroid::Initialize() {
  // The address |_etext| gives the end of the .text section in the binary. This
  // value is more accurate than parsing the memory map since the mapped
  // regions are usualy larger than the .text section.
  executable_end_addr_ = reinterpret_cast<uintptr_t>(&_etext);
  // The address of |__executable_start| gives the start address of the
  // executable. This value is used to find the offset address of the
  // instruction in binary from PC.
  executable_start_addr_ = reinterpret_cast<uintptr_t>(&__executable_start);

  // This file name is defined by extract_unwind_tables.gni.
  static constexpr char kCfiFileName[] = "assets/unwind_cfi";
  MemoryMappedFile::Region cfi_region;
  int fd = base::android::OpenApkAsset(kCfiFileName, &cfi_region);
  if (fd < 0)
    return;
  cfi_mmap_ = std::make_unique<MemoryMappedFile>();
  // The CFI region starts at |cfi_region.offset|.
  if (!cfi_mmap_->Initialize(base::File(fd), cfi_region))
    return;
  // The CFI file should contain rows of 8 bytes each.
  DCHECK_EQ(0u, cfi_region.size % sizeof(CFIUnwindInfo));
  unwind_table_row_count_ = cfi_region.size / sizeof(CFIUnwindInfo);
  can_unwind_stack_frames_ = true;
}

size_t CFIBacktraceAndroid::Unwind(const void** out_trace,
                                   size_t max_depth) const {
  // This function walks the stack using the call frame information to find the
  // return addresses of all the functions that belong to current binary in call
  // stack. For each function the CFI table defines the offset of the previous
  // call frame and offset where the return address is stored.
  if (!can_unwind_stack_frames())
    return 0;

  // Get the current register state. This register state can be taken at any
  // point in the function and the unwind information would be for this point.
  // Define local variables before trying to get the current PC and SP to make
  // sure the register state obtained is consistent with each other.
  uintptr_t pc = 0, sp = 0;
  asm volatile("mov %0, pc" : "=r"(pc));
  asm volatile("mov %0, sp" : "=r"(sp));

  // We can only unwind as long as the pc is within the chrome.so.
  size_t depth = 0;
  while (pc > executable_start_addr_ && pc <= executable_end_addr_ &&
         depth < max_depth) {
    out_trace[depth++] = reinterpret_cast<void*>(pc);
    // The offset of function from the start of the chrome.so binary:
    uintptr_t func_addr = pc - executable_start_addr_;
    CFIRow cfi{};
    if (!FindCFIRowForPC(func_addr, &cfi))
      break;

    // The rules for unwinding using the CFI information are:
    // SP_prev = SP_cur + cfa_offset and
    // PC_prev = * (SP_prev - ra_offset).
    sp = sp + cfi.cfa_offset;
    memcpy(&pc, reinterpret_cast<uintptr_t*>(sp - cfi.ra_offset),
           sizeof(uintptr_t));
  }
  return depth;
}

bool CFIBacktraceAndroid::FindCFIRowForPC(
    uintptr_t func_addr,
    CFIBacktraceAndroid::CFIRow* cfi) const {
  // Consider the CFI mapped region as an array of CFIUnwindInfo since each row
  // is 8 bytes long and it contains |cfi_region_size_| / 8 rows. We define
  // start and end iterator on this array and use std::lower_bound() to binary
  // search on this array. std::lower_bound() returns the row that corresponds
  // to the first row that has address greater than the current value, since
  // address is used in compartor.
  const CFIUnwindInfo* start =
      reinterpret_cast<const CFIUnwindInfo*>(cfi_mmap_->data());
  const CFIUnwindInfo* end = start + unwind_table_row_count_;
  const CFIUnwindInfo to_find = {func_addr, {0}};
  const CFIUnwindInfo* found = std::lower_bound(
      start, end, to_find,
      [](const auto& a, const auto& b) { return a.addr < b.addr; });
  *cfi = {0};

  // The given address is less than the start address in the CFI table if
  // lower_bound() returns start.
  if (found == start)
    return false;
  // If the given address is equal to the found address, then use the found row.
  // Otherwise the required row is always one less than the value returned by
  // std::lower_bound().
  if (found == end || found->address() != func_addr)
    found--;

  DCHECK_LE(found->address(), func_addr);
  DCHECK(!found->is_function_type())
      << "Current PC cannot be start of a function";

  // The CFIUnwindInfo::data field hold the CFI information since the row
  // found should not correspond to function start address. So, interpret the
  // data in the found row as CFI data which contains the CFA and RA offsets.
  *cfi = {found->cfa_offset(), found->ra_offset()};
  DCHECK(cfi->cfa_offset);

  // Find the function data for the current row by iterating till we reach the a
  // row of type FUNCTION, to check if the unwind information is valid. Also
  // find the RA offset if we do not have it in the CFI row found.
  const CFIUnwindInfo* it = found;
  for (; !it->is_function_type() && it >= start; it--) {
    // If ra offset of the last specified row should be used, if unspecified.
    // TODO(ssid): This should be fixed in the format and we should always
    // output ra offset.
    if (!cfi->ra_offset)
      cfi->ra_offset = it->ra_offset();
  }
  // If the given function adddress does not belong to the function found, then
  // the unwind info is invalid.
  if (!it->is_instruction_in_function(func_addr))
    return false;

  DCHECK(cfi->ra_offset);
  return true;
}

}  // namespace trace_event
}  // namespace base
