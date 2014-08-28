// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_RESOURCE_MANAGER_PUBLIC_RESOURCE_MANAGER_DLEGATE_H_
#define ATHENA_RESOURCE_MANAGER_PUBLIC_RESOURCE_MANAGER_DLEGATE_H_

#include <string>

#include "base/macros.h"

namespace athena {

// The resource manager delegate which abstracts system function calls to allow
// unit tests to override them.
class ResourceManagerDelegate {
 public:
  static ResourceManagerDelegate* CreateResourceManagerDelegate();

  ResourceManagerDelegate() {}
  virtual ~ResourceManagerDelegate() {}

  // Returns the percentage of memory used in the system.
  virtual int GetUsedMemoryInPercent() = 0;

  // Returns the time memory pressure interval time in ms to be used by the
  // memory pressure monitoring system.
  virtual int MemoryPressureIntervalInMS() = 0;
};

}  // namespace athena

#endif  // ATHENA_RESOURCE_MANAGER_PUBLIC_RESOURCE_MANAGER_DLEGATE_H_
