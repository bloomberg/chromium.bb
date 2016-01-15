// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/browser/power_save_blocker_impl.h"

namespace content {

PowerSaveBlocker::~PowerSaveBlocker() {}

// static
scoped_ptr<PowerSaveBlocker> PowerSaveBlocker::Create(
    PowerSaveBlockerType type,
    Reason reason,
    const std::string& description) {
#if defined(OS_ANDROID) && defined(USE_AURA)
  return nullptr;
#else
  return scoped_ptr<PowerSaveBlocker>(
      new PowerSaveBlockerImpl(type, reason, description));
#endif
}

}  // namespace content
