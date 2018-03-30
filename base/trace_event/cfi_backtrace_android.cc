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

The unwind table file has 2 tables UNW_INDEX and UNW_DATA, inspired from ARM
EHABI format. The first table contains function addresses and an index into the
UNW_DATA table. The second table contains one or more rows for the function
unwind information.

The output file starts with 4 bytes counting the size of UNW_INDEX in bytes.
Then UNW_INDEX table and UNW_DATA table.
UNW_INDEX contains one row for each function. Each row is 6 bytes long:
  4 bytes: Function start address.
  2 bytes: offset (in count of 2 bytes) of function data from start of UNW_DATA.
The last entry in the table always contains CANT_UNWIND index to specify the
end address of the last function.

UNW_DATA contains data of all the functions. Each function data contains N rows.
The data found at the address pointed from UNW_INDEX will be:
  2 bytes: N - number of rows that belong to current function.
  N * 4 bytes: N rows of data. 16 bits : Address offset from function start.
                               14 bits : CFA offset / 4.
                                2 bits : RA offset / 4.
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

// The value of index when the function does not have unwind information.
constexpr uint32_t kCantUnwind = 0xFFFF;

// The mask on the CFI row data that is used to get the high 14 bits and
// multiply it by 4 to get CFA offset. Since the last 2 bits are masked out, a
// shift is not necessary.
constexpr uint16_t kCFAMask = 0xfffc;

// The mask on the CFI row data that is used to get the low 2 bits and multiply
// it by 4 to get the RA offset.
constexpr uint16_t kRAMask = 0x3;
constexpr uint16_t kRAShift = 2;

// The code in this file assumes we are running in 32-bit builds since all the
// addresses in the unwind table are specified in 32 bits.
static_assert(sizeof(uintptr_t) == 4,
              "The unwind table format is only valid for 32 bit builds.");

// The struct that corresponds to each row in the UNW_INDEX table. The first 4
// bytes in the row represents the address of the function w.r.t. to the start
// of the binary and the next 2 bytes have the index. The members of this struct
// is in order of the input format. We cast the memory map of the unwind table
// as an array of CFIUnwindIndexRow and use it to read data and search. So, the
// size of this struct should be 6 bytes and the order of the members is fixed
// according to the given format.
struct CFIUnwindIndexRow {
  // Declare all the members of the function with size 2 bytes to make sure the
  // alignment is 2 bytes and the struct is not padded to 8 bytes. The |addr_l|
  // and |addr_r| represent the lower and higher 2 bytes of the function
  // address.
  uint16_t addr_l;
  uint16_t addr_r;

  // The |index| is count in terms of 2 byte address into the UNW_DATA table,
  // where the CFI data of the function exists.
  uint16_t index;

  // Returns the address of the function as offset from the start of the binary,
  // to which the index row corresponds to.
  uintptr_t addr() const { return (addr_r << 16) | addr_l; }
};

// The CFI data in UNW_DATA table starts with number of rows (N) and then
// followed by N rows of 4 bytes long. The CFIUnwindDataRow represents a single
// row of CFI data of a function in the table. Since we cast the memory at the
// address after the address of number of rows, into an array of
// CFIUnwindDataRow, the size of the struct should be 4 bytes and the order of
// the members is fixed according to the given format. The first 2 bytes tell
// the address of function and last 2 bytes give the CFI data for the offset.
struct CFIUnwindDataRow {
  // The address of the instruction in terms of offset from the start of the
  // function.
  uint16_t addr_offset;
  // Represents the CFA and RA offsets to get information about next stack
  // frame. This is the CFI data at the point before executing the instruction
  // at |addr_offset| from the start of the function.
  uint16_t cfi_data;

  // Return the RA offset for the current unwind row.
  size_t ra_offset() const { return (cfi_data & kRAMask) << kRAShift; }

  // Returns the CFA offset for the current unwind row.
  size_t cfa_offset() const { return cfi_data & kCFAMask; }
};

static_assert(
    sizeof(CFIUnwindIndexRow) == 6,
    "The CFIUnwindIndexRow struct must be exactly 6 bytes for searching.");

static_assert(
    sizeof(CFIUnwindDataRow) == 4,
    "The CFIUnwindDataRow struct must be exactly 4 bytes for searching.");

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

  // The first 4 bytes in the file is the size of UNW_INDEX table. The UNW_INDEX
  // table contains rows of 6 bytes each.
  size_t unw_index_size = 0;
  memcpy(&unw_index_size, cfi_mmap_->data(), sizeof(unw_index_size));
  DCHECK_EQ(0u, unw_index_size % sizeof(CFIUnwindIndexRow));
  DCHECK_GT(cfi_region.size, unw_index_size);
  unw_index_start_addr_ =
      reinterpret_cast<const size_t*>(cfi_mmap_->data()) + 1;
  unw_index_row_count_ = unw_index_size / sizeof(CFIUnwindIndexRow);

  // The UNW_DATA table data is right after the end of UNW_INDEX table.
  // Interpret the UNW_DATA table as an array of 2 byte numbers since the
  // indexes we have from the UNW_INDEX table are in terms of 2 bytes.
  unw_data_start_addr_ = reinterpret_cast<const uint16_t*>(
      reinterpret_cast<uintptr_t>(unw_index_start_addr_) + unw_index_size);
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
  // Consider the UNW_TABLE as an array of CFIUnwindIndexRow since each row
  // is 6 bytes long and it contains |unw_index_size_| / 6 rows. We define
  // start and end iterator on this array and use std::lower_bound() to binary
  // search on this array. std::lower_bound() returns the row that corresponds
  // to the first row that has address greater than the current value, since
  // address is used in compartor.
  const CFIUnwindIndexRow* start =
      static_cast<const CFIUnwindIndexRow*>(unw_index_start_addr_);
  const CFIUnwindIndexRow* end = start + unw_index_row_count_;
  const CFIUnwindIndexRow to_find = {func_addr & 0xffff, func_addr >> 16, 0};
  const CFIUnwindIndexRow* found = std::lower_bound(
      start, end, to_find,
      [](const auto& a, const auto& b) { return a.addr() < b.addr(); });
  *cfi = {0};

  // If found is start, then the given function is not in the table. If the
  // given pc is start of a function then we cannot unwind.
  if (found == start || found->addr() == func_addr)
    return false;

  // The required row is always one less than the value returned by
  // std::lower_bound().
  found--;
  uintptr_t func_start_addr = found->addr();
  DCHECK_LE(func_start_addr, func_addr);
  // If the index is CANT_UNWIND then we do not have unwind infomation for the
  // function.
  if (found->index == kCantUnwind)
    return false;

  // The unwind data for the current function is at an offsset of the index
  // found in UNW_INDEX table.
  const uint16_t* unwind_data = unw_data_start_addr_ + found->index;
  // The value of first 2 bytes is the CFI data row count for the function.
  uint16_t row_count = 0;
  memcpy(&row_count, unwind_data, sizeof(row_count));
  // And the actual CFI rows start after 2 bytes from the |unwind_data|. Cast
  // the data into an array of CFIUnwindDataRow since the struct is designed to
  // represent each row. We should be careful to read only |row_count| number of
  // elements in the array.
  const CFIUnwindDataRow* function_data =
      reinterpret_cast<const CFIUnwindDataRow*>(unwind_data + 1);

  // Iterate through the CFI rows of the function to find the row that gives
  // offset for the given instruction address.
  CFIUnwindDataRow cfi_row = {0, 0};
  uint16_t ra_offset = 0;
  for (uint16_t i = 0; i < row_count; ++i) {
    CFIUnwindDataRow row;
    memcpy(&row, function_data + i, sizeof(CFIUnwindDataRow));
    // The return address of the function is the instruction that is not yet
    // been executed. The cfi row specifies the unwind info before executing the
    // given instruction. If the given address is equal to the instruction
    // offset, then use the current row. Or use the row with highest address
    // less than the given address.
    if (row.addr_offset + func_start_addr > func_addr)
      break;

    cfi_row = row;
    // The ra offset of the last specified row should be used, if unspecified.
    // So, keep updating the RA offset till we reach the correct CFI row.
    // TODO(ssid): This should be fixed in the format and we should always
    // output ra offset.
    if (cfi_row.ra_offset())
      ra_offset = cfi_row.ra_offset();
  }
  DCHECK_NE(0u, cfi_row.addr_offset);
  *cfi = {cfi_row.cfa_offset(), ra_offset};
  DCHECK(cfi->cfa_offset);
  DCHECK(cfi->ra_offset);
  return true;
}

}  // namespace trace_event
}  // namespace base
