// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/cfi_backtrace_android.h"

#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

namespace {

void* GetPC() {
  return __builtin_return_address(0);
}

}  // namespace

TEST(CFIBacktraceAndroidTest, TestUnwinding) {
  auto* unwinder = CFIBacktraceAndroid::GetInstance();
  EXPECT_TRUE(unwinder->can_unwind_stack_frames());
  EXPECT_GT(unwinder->executable_start_addr_, 0u);
  EXPECT_GT(unwinder->executable_end_addr_, unwinder->executable_start_addr_);
  EXPECT_GT(unwinder->cfi_mmap_->length(), 0u);

  const size_t kMaxFrames = 100;
  const void* frames[kMaxFrames];
  size_t unwind_count = unwinder->Unwind(frames, kMaxFrames);
  // Expect at least 2 frames in the result.
  ASSERT_GT(unwind_count, 2u);
  EXPECT_LE(unwind_count, kMaxFrames);

  const size_t kMaxCurrentFuncCodeSize = 50;
  const uintptr_t current_pc = reinterpret_cast<uintptr_t>(GetPC());
  const uintptr_t actual_frame = reinterpret_cast<uintptr_t>(frames[2]);
  EXPECT_NEAR(current_pc, actual_frame, kMaxCurrentFuncCodeSize);

  for (size_t i = 0; i < unwind_count; ++i) {
    EXPECT_GT(reinterpret_cast<uintptr_t>(frames[i]),
              unwinder->executable_start_addr_);
    EXPECT_LT(reinterpret_cast<uintptr_t>(frames[i]),
              unwinder->executable_end_addr_);
  }
}

TEST(CFIBacktraceAndroidTest, TestFindCFIRow) {
  auto* unwinder = CFIBacktraceAndroid::GetInstance();
  /* Input is generated from the CFI file:
  STACK CFI INIT 1000 500
  STACK CFI 1002 .cfa: sp 272 + .ra: .cfa -4 + ^ r4: .cfa -16 +
  STACK CFI 1008 .cfa: sp 544 + .r1: .cfa -0 + ^ r4: .cfa -16 + ^
  STACK CFI 1040 .cfa: sp 816 + .r1: .cfa -0 + ^ r4: .cfa -16 + ^
  STACK CFI 1050 .cfa: sp 816 + .ra: .cfa -8 + ^ r4: .cfa -16 + ^
  STACK CFI 1080 .cfa: sp 544 + .r1: .cfa -0 + ^ r4: .cfa -16 + ^

  STACK CFI INIT 2000 22
  STACK CFI 2004 .cfa: sp 16 + .ra: .cfa -12 + ^ r4: .cfa -16 + ^
  STACK CFI 2008 .cfa: sp 16 + .ra: .cfa -12 + ^ r4: .cfa -16 + ^

  STACK CFI INIT 2024 100
  STACK CFI 2030 .cfa: sp 48 + .ra: .cfa -12 + ^ r4: .cfa -16 + ^
  STACK CFI 2100 .cfa: sp 64 + .r1: .cfa -0 + ^ r4: .cfa -16 + ^

  STACK CFI INIT 2200 10
  STACK CFI 2204 .cfa: sp 44 + .ra: .cfa -8 + ^ r4: .cfa -16 + ^
  */
  uint16_t input[] = {0x2A,   0x0,    0x1000, 0x0,    0x0,    0x1502, 0x0,
                      0xffff, 0x2000, 0x0,    0xb,    0x2024, 0x0,    0x10,
                      0x2126, 0x0,    0xffff, 0x2200, 0x0,    0x15,   0x2212,
                      0x0,    0xffff, 0x5,    0x2,    0x111,  0x8,    0x220,
                      0x40,   0x330,  0x50,   0x332,  0x80,   0x220,  0x2,
                      0x4,    0x13,   0x8,    0x13,   0x2,    0xc,    0x33,
                      0xdc,   0x40,   0x1,    0x4,    0x2e};
  FilePath temp_path;
  CreateTemporaryFile(&temp_path);
  EXPECT_EQ(
      static_cast<int>(sizeof(input)),
      WriteFile(temp_path, reinterpret_cast<char*>(input), sizeof(input)));

  unwinder->cfi_mmap_.reset(new MemoryMappedFile());
  unwinder->cfi_mmap_->Initialize(temp_path);
  unwinder->unw_index_start_addr_ =
      reinterpret_cast<const size_t*>(unwinder->cfi_mmap_->data()) + 1;
  unwinder->unw_index_row_count_ = input[0] / 6;
  unwinder->unw_data_start_addr_ = reinterpret_cast<const uint16_t*>(
      reinterpret_cast<uintptr_t>(unwinder->unw_index_start_addr_) + input[0]);

  CFIBacktraceAndroid::CFIRow cfi_row = {0};
  EXPECT_FALSE(unwinder->FindCFIRowForPC(0x00, &cfi_row));
  EXPECT_FALSE(unwinder->FindCFIRowForPC(0x100, &cfi_row));
  EXPECT_FALSE(unwinder->FindCFIRowForPC(0x1502, &cfi_row));
  EXPECT_FALSE(unwinder->FindCFIRowForPC(0x3000, &cfi_row));
  EXPECT_FALSE(unwinder->FindCFIRowForPC(0x2024, &cfi_row));
  EXPECT_FALSE(unwinder->FindCFIRowForPC(0x2212, &cfi_row));

  const CFIBacktraceAndroid::CFIRow kRow1 = {0x110, 0x4};
  const CFIBacktraceAndroid::CFIRow kRow2 = {0x220, 0x4};
  const CFIBacktraceAndroid::CFIRow kRow3 = {0x220, 0x8};
  const CFIBacktraceAndroid::CFIRow kRow4 = {0x30, 0xc};
  const CFIBacktraceAndroid::CFIRow kRow5 = {0x2c, 0x8};
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1002, &cfi_row));
  EXPECT_EQ(kRow1, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1003, &cfi_row));
  EXPECT_EQ(kRow1, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1008, &cfi_row));
  EXPECT_EQ(kRow2, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1009, &cfi_row));
  EXPECT_EQ(kRow2, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1039, &cfi_row));
  EXPECT_EQ(kRow2, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1080, &cfi_row));
  EXPECT_EQ(kRow3, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1100, &cfi_row));
  EXPECT_EQ(kRow3, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x2050, &cfi_row));
  EXPECT_EQ(kRow4, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x2208, &cfi_row));
  EXPECT_EQ(kRow5, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x2210, &cfi_row));
  EXPECT_EQ(kRow5, cfi_row);
}

}  // namespace trace_event
}  // namespace base
