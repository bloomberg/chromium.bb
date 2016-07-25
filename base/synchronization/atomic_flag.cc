// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/atomic_flag.h"

#include "base/logging.h"

namespace base {

AtomicFlag::AtomicFlag() = default;

void AtomicFlag::Set() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::subtle::Release_Store(&flag_, 1);
}

bool AtomicFlag::IsSet() const {
  return base::subtle::Acquire_Load(&flag_) != 0;
}

void AtomicFlag::UnsafeResetForTesting() {
  base::subtle::Release_Store(&flag_, 0);
}

}  // namespace base
