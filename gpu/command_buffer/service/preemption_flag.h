// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_PREEMPTION_FLAG_H_
#define GPU_COMMAND_BUFFER_SERVICE_PREEMPTION_FLAG_H_

#include "base/atomic_ref_count.h"
#include "base/atomicops.h"
#include "base/memory/ref_counted.h"
#include "gpu/gpu_export.h"

namespace gpu {

class PreemptionFlag : public base::RefCountedThreadSafe<PreemptionFlag> {
 public:
  PreemptionFlag() : flag_(0) {}

  bool IsSet() { return !base::AtomicRefCountIsZero(&flag_); }
  void Set() { base::AtomicRefCountInc(&flag_); }
  void Reset() { base::subtle::NoBarrier_Store(&flag_, 0); }

 private:
  base::AtomicRefCount flag_;

  ~PreemptionFlag() {}

  friend class base::RefCountedThreadSafe<PreemptionFlag>;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_PREEMPTION_FLAG_H_
