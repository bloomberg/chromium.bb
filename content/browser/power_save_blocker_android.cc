// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker_impl.h"

#include "base/logging.h"

namespace content {

class PowerSaveBlockerImpl::Delegate
    : public base::RefCounted<PowerSaveBlockerImpl::Delegate> {
 private:
  friend class base::RefCounted<Delegate>;
  ~Delegate() {}
};

PowerSaveBlockerImpl::PowerSaveBlockerImpl(PowerSaveBlockerType type,
                                           const std::string& reason) {
  // TODO(wangxianzhu): Implement it.
  // This may be called on any thread.
  NOTIMPLEMENTED();
}

PowerSaveBlockerImpl::~PowerSaveBlockerImpl() {
}

}  // namespace content
