// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/tools/crash_service/caps/process_singleton_win.h"

namespace caps {

ProcessSingleton::ProcessSingleton() : mutex_(nullptr) {
  auto mutex = ::CreateMutex(nullptr, TRUE, L"CHROME.CAPS.V1");
  if (!mutex)
    return;
  if (::GetLastError() == ERROR_ALREADY_EXISTS) {
    ::CloseHandle(mutex);
    return;
  }
  // We are now the single instance.
  mutex_ = mutex;
}

ProcessSingleton::~ProcessSingleton() {
  if (mutex_)
    ::CloseHandle(mutex_);
}

}  // namespace caps

