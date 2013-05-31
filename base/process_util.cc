// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

namespace base {

bool KillProcesses(const FilePath::StringType& executable_name, int exit_code,
                   const ProcessFilter* filter) {
  bool result = true;
  NamedProcessIterator iter(executable_name, filter);
  while (const ProcessEntry* entry = iter.NextProcessEntry()) {
#if defined(OS_WIN)
    result &= KillProcessById(entry->pid(), exit_code, true);
#else
    result &= KillProcess(entry->pid(), exit_code, true);
#endif
  }
  return result;
}

}  // namespace base
