// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TOOLS_CRASH_SERVICE_CAPS_PROCESS_SINGLETON_WIN_H_
#define CHROME_TOOLS_CRASH_SERVICE_CAPS_PROCESS_SINGLETON_WIN_H_

#include <windows.h>

#include "base/macros.h"

namespace caps {
// Uses a named mutex to make sure that only one process of
// with this particular version is launched.
class ProcessSingleton {
public:
  ProcessSingleton();
  ~ProcessSingleton();
  bool other_instance() const { return mutex_ == NULL; }

private:
  HANDLE mutex_;

  DISALLOW_COPY_AND_ASSIGN(ProcessSingleton);
};

}  // namespace caps

#endif  // CHROME_TOOLS_CRASH_SERVICE_CAPS_PROCESS_SINGLETON_WIN_H_

