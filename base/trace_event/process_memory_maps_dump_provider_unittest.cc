// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/process_memory_maps_dump_provider.h"

#include <fstream>
#include <sstream>

#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/process_memory_maps.h"
#include "base/trace_event/trace_event_argument.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

#if defined(OS_LINUX) || defined(OS_ANDROID)
TEST(ProcessMemoryMapsDumpProviderTest, ParseProcSmaps) {
  auto pmmdp = ProcessMemoryMapsDumpProvider::GetInstance();

  // Emulate a non-existent /proc/self/smaps.
  ProcessMemoryDump pmd_invalid;
  std::ifstream non_existent_file("/tmp/does-not-exist");
  ProcessMemoryMapsDumpProvider::proc_smaps_for_testing = &non_existent_file;
  CHECK_EQ(false, non_existent_file.good());
  pmmdp->DumpInto(&pmd_invalid);
  ASSERT_FALSE(pmd_invalid.has_process_mmaps());

  // Emulate an empty /proc/self/smaps.
  std::ifstream empty_file("/dev/null");
  ProcessMemoryMapsDumpProvider::proc_smaps_for_testing = &empty_file;
  CHECK_EQ(true, empty_file.good());
  pmmdp->DumpInto(&pmd_invalid);
  ASSERT_FALSE(pmd_invalid.has_process_mmaps());

  // TODO(primiano): in the next CLs add the code to parse some actual smaps
  // files and check that the parsing logic of the dump provider holds.
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

}  // namespace trace_event
}  // namespace base
