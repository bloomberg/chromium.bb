// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker_impl.h"

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace content {

// TODO(derat): Consider renaming this file; '_ozone' is a misnomer as power
// save is OS-specific, not display-system-specific.  This implementation
// ends up being used for non-ChromeOS Ozone platforms such as Chromecast.
// See crbug.com/495661 for more detail.
class PowerSaveBlockerImpl::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlockerImpl::Delegate> {
 public:
  Delegate() {}

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  virtual ~Delegate() {}

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

PowerSaveBlockerImpl::PowerSaveBlockerImpl(PowerSaveBlockerType type,
                                           Reason reason,
                                           const std::string& description)
    : delegate_(new Delegate()) {
}

PowerSaveBlockerImpl::~PowerSaveBlockerImpl() { }

}  // namespace content
