// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_MEMORY_COORDINATOR_PROXY_H_
#define BASE_MEMORY_MEMORY_COORDINATOR_PROXY_H_

#include "base/base_export.h"
#include "base/callback.h"
#include "base/memory/memory_coordinator_client.h"
#include "base/memory/singleton.h"

namespace base {

// The proxy of MemoryCoordinator to be accessed from components that is not
// in content/browser e.g. net.
class BASE_EXPORT MemoryCoordinatorProxy {
 public:
  using GetCurrentMemoryStateCallback = base::Callback<MemoryState()>;

  static MemoryCoordinatorProxy* GetInstance();

  // Returns the current memory state.
  MemoryState GetCurrentMemoryState() const;

  // Sets state-getter callback.
  void SetGetCurrentMemoryStateCallback(GetCurrentMemoryStateCallback callback);

 private:
  friend struct base::DefaultSingletonTraits<MemoryCoordinatorProxy>;

  MemoryCoordinatorProxy();
  virtual ~MemoryCoordinatorProxy();

  GetCurrentMemoryStateCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(MemoryCoordinatorProxy);
};

}  // namespace base

#endif  // BASE_MEMORY_MEMORY_COORDINATOR_PROXY_H_
