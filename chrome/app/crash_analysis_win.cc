// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/crash_analysis_win.h"

#include "breakpad/src/client/windows/handler/exception_handler.h"
#include "chrome/app/breakpad_win.h"

namespace {
// We start reporting at 400MB of executable memory.
size_t kAbnormalExecMemCutoff = 0x19000;

// To avoid resizing the vector of memory regions while in a crashed state we
// limit the size.
size_t kMaxMappings = 2048;
}

extern "C" void DumpProcessAbnormalSignature();

CrashAnalysis::CrashAnalysis()
    : exec_pages_(0) {
  memory_regions_.reserve(kMaxMappings);
}

// Alayze a crash to determine whether reporting additional crash details is
// desirable. Specifcally we are interested in crashes which appear
// particularly exploitable or which appear to be failed attempts at
// exploitation.
void CrashAnalysis::Analyze(_EXCEPTION_POINTERS* info) {
  // Get system page size.
  SYSTEM_INFO system_info = {0};
  GetSystemInfo(&system_info);
  DWORD prev_protection = NULL;
  MEMORY_BASIC_INFORMATION mbi = {0};
  void* base_address = NULL;
  // Build a vector of the current memory mappings for the process.
  // This will allow us to easily sample memory looking for the various
  // signatures and patterns which interest us.
  while (VirtualQuery(base_address, &mbi, sizeof(mbi)) ==
      sizeof(mbi)) {
    if (mbi.State == MEM_COMMIT && memory_regions_.size() < kMaxMappings) {
      memory_regions_.push_back(mbi);
      if (mbi.Protect &
          (PAGE_EXECUTE || PAGE_EXECUTE_READ || PAGE_EXECUTE_READWRITE)) {
        exec_pages_ += mbi.RegionSize / system_info.dwPageSize;
        if (mbi.Protect & PAGE_EXECUTE_READWRITE &&
            !(prev_protection &
                (PAGE_GUARD || PAGE_NOACCESS || PAGE_EXECUTE_READWRITE))) {
          // There should never be a write/execute mapping without a guard page
          // in front. Send the report and quit early.
          DumpProcessAbnormalSignature();
          return;
        }
      }
      prev_protection = mbi.Protect;
    }
    void* new_base = (static_cast<BYTE*>(mbi.BaseAddress)) + mbi.RegionSize;
    base_address = new_base;
  }

  // We trigger upload if the process has mapped more than the threashold of
  // executable memory.
  if (exec_pages_ > kAbnormalExecMemCutoff) {
    DumpProcessAbnormalSignature();
    return;
  }

  // TODO(cdn): Moar checks for abnormality.
  return;
}
