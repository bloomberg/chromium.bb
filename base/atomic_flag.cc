// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/atomic_flag.h"

#include "base/dynamic_annotations.h"
#include "base/logging.h"

namespace base {

void AtomicFlag::Set() {
  DCHECK(!flag_);
  // Set() method can't be called if the flag has already been set.

  ANNOTATE_HAPPENS_BEFORE(&flag_);
  base::subtle::Release_Store(&flag_, 1);
}

bool AtomicFlag::IsSet() const {
  bool ret = base::subtle::Acquire_Load(&flag_) != 0;
  if (ret) {
    ANNOTATE_HAPPENS_AFTER(&flag_);
  }
  return ret;
}

}  // namespace base
