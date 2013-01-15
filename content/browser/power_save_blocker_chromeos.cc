// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker_impl.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/power/power_state_override.h"
#include "content/public/browser/browser_thread.h"

namespace content {

class PowerSaveBlockerImpl::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlockerImpl::Delegate> {
 public:
  Delegate(PowerSaveBlockerType type) : type_(type) {}

  // Creates a PowerStateOverride object to override the default power
  // management behavior.
  void ApplyBlock() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    chromeos::PowerStateOverride::Mode mode =
        chromeos::PowerStateOverride::BLOCK_SYSTEM_SUSPEND;
    switch (type_) {
      case kPowerSaveBlockPreventAppSuspension:
        mode = chromeos::PowerStateOverride::BLOCK_SYSTEM_SUSPEND;
        break;
      case kPowerSaveBlockPreventDisplaySleep:
        mode = chromeos::PowerStateOverride::BLOCK_DISPLAY_SLEEP;
        break;
      default:
        NOTREACHED() << "Unhandled block type " << type_;
    }
    override_ = new chromeos::PowerStateOverride(mode);
  }

  // Resets the previously-created PowerStateOverride object.
  void RemoveBlock() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    override_ = NULL;
  }

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  virtual ~Delegate() {}

  PowerSaveBlockerType type_;

  scoped_refptr<chromeos::PowerStateOverride> override_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

PowerSaveBlockerImpl::PowerSaveBlockerImpl(PowerSaveBlockerType type,
                                           const std::string& reason)
    : delegate_(new Delegate(type)) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&Delegate::ApplyBlock, delegate_));
}

PowerSaveBlockerImpl::~PowerSaveBlockerImpl() {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&Delegate::RemoveBlock, delegate_));
}

}  // namespace content
