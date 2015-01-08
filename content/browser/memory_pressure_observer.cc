// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/memory_pressure_observer.h"

#include "content/browser/browser_main_loop.h"

#if defined(OS_CHROMEOS)
namespace content {

base::MemoryPressureObserverChromeOS* GetMemoryPressureObserver() {
  DCHECK(BrowserMainLoop::GetInstance());
  return BrowserMainLoop::GetInstance()->memory_pressure_observer();
}

}  // namespace content
#endif
