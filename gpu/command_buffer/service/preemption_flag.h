// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_PREEMPTION_FLAG_H_
#define GPU_COMMAND_BUFFER_SERVICE_PREEMPTION_FLAG_H_

#include <atomic>

#include "base/memory/ref_counted.h"
#include "gpu/gpu_export.h"

namespace gpu {

// Boolean flag that can be set, unset and checked.
// IsSet acquires changes to memory published by Set/Reset.
class PreemptionFlag : public base::RefCountedThreadSafe<PreemptionFlag> {
 public:
  bool IsSet() const { return flag_.load(std::memory_order_acquire) != 0; }
  void Set() { flag_.store(1, std::memory_order_release); }
  void Reset() { flag_.store(0, std::memory_order_release); }

 private:
  ~PreemptionFlag() {}

  std::atomic_int flag_{0};

  friend class base::RefCountedThreadSafe<PreemptionFlag>;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_PREEMPTION_FLAG_H_
