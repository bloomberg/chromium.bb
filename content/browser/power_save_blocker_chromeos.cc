// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker_impl.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

// Converts a PowerSaveBlocker::Reason to a
// chromeos::PowerPolicyController::WakeLockReason.
chromeos::PowerPolicyController::WakeLockReason GetWakeLockReason(
    PowerSaveBlocker::Reason reason) {
  switch (reason) {
    case PowerSaveBlocker::kReasonAudioPlayback:
      return chromeos::PowerPolicyController::REASON_AUDIO_PLAYBACK;
    case PowerSaveBlocker::kReasonVideoPlayback:
      return chromeos::PowerPolicyController::REASON_VIDEO_PLAYBACK;
    case PowerSaveBlocker::kReasonOther:
      return chromeos::PowerPolicyController::REASON_OTHER;
  }
  return chromeos::PowerPolicyController::REASON_OTHER;
}

}  // namespace

class PowerSaveBlockerImpl::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlockerImpl::Delegate> {
 public:
  Delegate(PowerSaveBlockerType type,
           Reason reason,
           const std::string& description)
      : type_(type), reason_(reason), description_(description), block_id_(0) {}

  void ApplyBlock() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!chromeos::PowerPolicyController::IsInitialized())
      return;

    auto* controller = chromeos::PowerPolicyController::Get();
    switch (type_) {
      case kPowerSaveBlockPreventAppSuspension:
        block_id_ = controller->AddSystemWakeLock(GetWakeLockReason(reason_),
                                                  description_);
        break;
      case kPowerSaveBlockPreventDisplaySleep:
        block_id_ = controller->AddScreenWakeLock(GetWakeLockReason(reason_),
                                                  description_);
        break;
      default:
        NOTREACHED() << "Unhandled block type " << type_;
    }
  }

  void RemoveBlock() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!chromeos::PowerPolicyController::IsInitialized())
      return;

    chromeos::PowerPolicyController::Get()->RemoveWakeLock(block_id_);
  }

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  virtual ~Delegate() {}

  PowerSaveBlockerType type_;
  Reason reason_;
  std::string description_;

  // ID corresponding to the block request in PowerPolicyController.
  int block_id_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

PowerSaveBlockerImpl::PowerSaveBlockerImpl(PowerSaveBlockerType type,
                                           Reason reason,
                                           const std::string& description)
    : delegate_(new Delegate(type, reason, description)) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&Delegate::ApplyBlock, delegate_));
}

PowerSaveBlockerImpl::~PowerSaveBlockerImpl() {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&Delegate::RemoveBlock, delegate_));
}

}  // namespace content
