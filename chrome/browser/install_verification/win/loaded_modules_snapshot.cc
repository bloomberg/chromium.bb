// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/install_verification/win/loaded_modules_snapshot.h"

#include <Psapi.h>

#include "base/logging.h"

bool GetLoadedModulesSnapshot(std::vector<HMODULE>* snapshot) {
  DCHECK(snapshot);
  DCHECK_EQ(0u, snapshot->size());
  snapshot->resize(1);

  HANDLE process = ::GetCurrentProcess();

  // We will retry at least once after first determining |bytes_required|. If
  // the list of modules changes after we receive |bytes_required| we may retry
  // more than once.
  int retries_remaining = 5;
  do {
    DWORD bytes_required = 0;
    // EnumProcessModules returns 'success' even if the buffer size is too
    // small.
    if (!::EnumProcessModules(process,
                              &(*snapshot)[0],
                              snapshot->size() * sizeof(HMODULE),
                              &bytes_required)) {
      DPLOG(ERROR) << "::EnumProcessModules failed.";
      return false;
    }
    DCHECK_EQ(0u, bytes_required % sizeof(HMODULE));
    size_t num_modules = bytes_required / sizeof(HMODULE);
    if (num_modules <= snapshot->size()) {
      // Buffer size was too big, presumably because a module was unloaded.
      snapshot->erase(snapshot->begin() + num_modules, snapshot->end());
      return true;
    } else if (num_modules == 0) {
      DLOG(ERROR) << "Can't determine the module list size.";
      return false;
    } else {
      // Buffer size was too small. Try again with a larger buffer.
      snapshot->resize(num_modules, NULL);
    }
  } while (--retries_remaining);

  DLOG(ERROR) << "Failed to enumerate modules.";
  return false;
}
