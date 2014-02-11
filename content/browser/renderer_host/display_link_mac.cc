// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/display_link_mac.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"

namespace base {

template<>
struct ScopedTypeRefTraits<CVDisplayLinkRef> {
  static void Retain(CVDisplayLinkRef object) {
    CVDisplayLinkRetain(object);
  }
  static void Release(CVDisplayLinkRef object) {
    CVDisplayLinkRelease(object);
  }
};

}  // namespace base

namespace content {

// static
scoped_refptr<DisplayLinkMac> DisplayLinkMac::Create() {
  CVReturn ret = kCVReturnSuccess;

  base::ScopedTypeRef<CVDisplayLinkRef> display_link;
  ret = CVDisplayLinkCreateWithActiveCGDisplays(display_link.InitializeInto());
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkCreateWithActiveCGDisplays failed: " << ret;
    return NULL;
  }

  scoped_refptr<DisplayLinkMac> display_link_mac;
  display_link_mac = new DisplayLinkMac(display_link);

  ret = CVDisplayLinkSetOutputCallback(
      display_link_mac->display_link_,
      &DisplayLinkCallback,
      display_link_mac.get());
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkSetOutputCallback failed: " << ret;
    return NULL;
  }

  return display_link_mac;
}

DisplayLinkMac::DisplayLinkMac(
    base::ScopedTypeRef<CVDisplayLinkRef> display_link)
      : display_link_(display_link),
        stop_timer_(
            FROM_HERE, base::TimeDelta::FromSeconds(1),
            this, &DisplayLinkMac::StopDisplayLink),
        timebase_and_interval_valid_(false) {
}

DisplayLinkMac::~DisplayLinkMac() {
  if (CVDisplayLinkIsRunning(display_link_))
    CVDisplayLinkStop(display_link_);
}

bool DisplayLinkMac::GetVSyncParameters(
    base::TimeTicks* timebase, base::TimeDelta* interval) {
  StartOrContinueDisplayLink();

  base::AutoLock lock(lock_);
  if (!timebase_and_interval_valid_)
    return false;

  *timebase = timebase_;
  *interval = interval_;
  return true;
}

void DisplayLinkMac::Tick(const CVTimeStamp* cv_time) {
  TRACE_EVENT0("browser", "DisplayLinkMac::GetVSyncParameters");
  base::AutoLock lock(lock_);

  // Verify that videoRefreshPeriod is 32 bits.
  DCHECK((cv_time->videoRefreshPeriod & ~0xffffFFFFull) == 0ull);

  // Verify that the numerator and denominator make some sense.
  uint32 numerator = static_cast<uint32>(cv_time->videoRefreshPeriod);
  uint32 denominator = cv_time->videoTimeScale;
  if (numerator <= 0 || denominator <= 0) {
    LOG(WARNING) << "Unexpected numerator or denominator, bailing.";
    return;
  }

  timebase_ = base::TimeTicks::FromInternalValue(
      cv_time->hostTime / 1000);
  interval_ = base::TimeDelta::FromMicroseconds(
      1000000 * static_cast<int64>(numerator) / denominator);
  timebase_and_interval_valid_ = true;
}

void DisplayLinkMac::StartOrContinueDisplayLink() {
  // Reset the timer, so that the display link won't be turned off for another
  // second.
  stop_timer_.Reset();

  if (CVDisplayLinkIsRunning(display_link_))
    return;

  CVReturn ret = CVDisplayLinkStart(display_link_);
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkStart failed: " << ret;
  }
}

void DisplayLinkMac::StopDisplayLink() {
  if (!CVDisplayLinkIsRunning(display_link_))
    return;

  CVReturn ret = CVDisplayLinkStop(display_link_);
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkStop failed: " << ret;
  }
}

CVReturn DisplayLinkMac::DisplayLinkCallback(
    CVDisplayLinkRef display_link,
    const CVTimeStamp* now,
    const CVTimeStamp* output_time,
    CVOptionFlags flags_in,
    CVOptionFlags* flags_out,
    void* context) {
  DisplayLinkMac* display_link_mac = static_cast<DisplayLinkMac*>(context);
  display_link_mac->Tick(output_time);
  return kCVReturnSuccess;
}

}  // content

