// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker_impl.h"

#include "base/logging.h"
#include "content/browser/android/content_video_view.h"
#include "content/public/browser/browser_thread.h"

namespace content {

class PowerSaveBlockerImpl::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlockerImpl::Delegate> {
 public:
  explicit Delegate(PowerSaveBlockerType type) : type_(type) {}

  // Does the actual work to apply or remove the desired power save block.
  void ApplyBlock();
  void RemoveBlock();

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  ~Delegate() {}

  // The counter of requests from clients for type
  // kPowerSaveBlockPreventDisplaySleep.
  static int blocker_count_;
  const PowerSaveBlockerType type_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

int PowerSaveBlockerImpl::Delegate::blocker_count_ = 0;

void PowerSaveBlockerImpl::Delegate::ApplyBlock() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (type_ != kPowerSaveBlockPreventDisplaySleep)
    return;

  if (blocker_count_ == 0)
    ContentVideoView::KeepScreenOn(true);
  ++blocker_count_;
}

void PowerSaveBlockerImpl::Delegate::RemoveBlock() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (type_ != kPowerSaveBlockPreventDisplaySleep)
    return;

  --blocker_count_;
  if (blocker_count_ == 0)
    ContentVideoView::KeepScreenOn(false);
}

PowerSaveBlockerImpl::PowerSaveBlockerImpl(PowerSaveBlockerType type,
                                           const std::string& reason)
    : delegate_(new Delegate(type)) {
  // This may be called on any thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Delegate::ApplyBlock, delegate_));
}

PowerSaveBlockerImpl::~PowerSaveBlockerImpl() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Delegate::RemoveBlock, delegate_));
}

}  // namespace content
