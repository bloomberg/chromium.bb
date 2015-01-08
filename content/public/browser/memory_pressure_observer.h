// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEMORY_PRESSURE_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_MEMORY_PRESSURE_OBSERVER_H_

#include "content/common/content_export.h"

#if defined(OS_CHROMEOS)
namespace base {
class MemoryPressureObserverChromeOS;
}

namespace content {

// Returns the MemoryPressureObserverChromeOS which gets created and is owned by
// content.
CONTENT_EXPORT base::MemoryPressureObserverChromeOS*
    GetMemoryPressureObserver();

}  // namespace content
#endif

#endif  // CONTENT_PUBLIC_BROWSER_MEMORY_PRESSURE_OBSERVER_H_
