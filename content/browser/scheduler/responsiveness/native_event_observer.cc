// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/native_event_observer.h"

#include "ui/events/platform/platform_event_source.h"

#if defined(OS_LINUX)
#if defined(USE_X11)
#include "ui/events/platform/x11/x11_event_source.h"  // nogncheck
#elif defined(USE_OZONE)
#include "ui/events/event.h"
#endif

#include "ui/events/platform_event.h"
#endif

namespace content {
namespace responsiveness {

NativeEventObserver::NativeEventObserver(
    WillRunEventCallback will_run_event_callback,
    DidRunEventCallback did_run_event_callback)
    : will_run_event_callback_(will_run_event_callback),
      did_run_event_callback_(did_run_event_callback) {
  RegisterObserver();
}

NativeEventObserver::~NativeEventObserver() {
  DeregisterObserver();
}

#if !defined(OS_MACOSX)
#if defined(OS_LINUX)
void NativeEventObserver::RegisterObserver() {
  ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
}
void NativeEventObserver::DeregisterObserver() {
  ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
}

void NativeEventObserver::WillProcessEvent(const ui::PlatformEvent& event) {
  will_run_event_callback_.Run(&event);
}

void NativeEventObserver::DidProcessEvent(const ui::PlatformEvent& event) {
#if defined(USE_OZONE)
  did_run_event_callback_.Run(&event, event->time_stamp());
#elif defined(USE_X11)
  // X11 uses a uint32_t on the wire protocol. Xlib casts this to an unsigned
  // long by prepending with 0s. We cast back to a uint32_t so that subtraction
  // works properly when the timestamp overflows back to 0.
  uint32_t event_server_time_ms =
      static_cast<uint32_t>(ui::X11EventSource::GetInstance()->GetTimestamp());
  uint32_t current_server_time_ms = static_cast<uint32_t>(
      ui::X11EventSource::GetInstance()->GetCurrentServerTime());

  int32_t delta_ms = current_server_time_ms - event_server_time_ms;

  // If for some reason server time is not monotonically increasing, ignore it.
  if (delta_ms < 0)
    delta_ms = 0;

  // Ignore pathologically long deltas. Server is probably having issues.
  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(delta_ms);
  base::TimeDelta long_duration = base::TimeDelta::FromSeconds(30);
  if (delta > long_duration)
    delta = base::TimeDelta();

  did_run_event_callback_.Run(&event, base::TimeTicks::Now() - delta);
#else
#error
#endif
}
#else   // defined(OS_LINUX)
// TODO(erikchen): Implement this for non-macOS, non-Linux platforms.
// https://crbug.com/859155.
void NativeEventObserver::RegisterObserver() {}
void NativeEventObserver::DeregisterObserver() {}
#endif  // defined(OS_LINUX)
#endif  // !defined(OS_MACOSX)

}  // namespace responsiveness
}  // namespace content
