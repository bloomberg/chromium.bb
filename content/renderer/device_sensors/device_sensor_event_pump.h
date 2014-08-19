// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_SENSOR_EVENT_PUMP_H_
#define CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_SENSOR_EVENT_PUMP_H_

#include "base/memory/shared_memory.h"
#include "base/timer/timer.h"
#include "content/public/renderer/platform_event_observer.h"

namespace content {

template <typename ListenerType>
class CONTENT_EXPORT DeviceSensorEventPump
    : NON_EXPORTED_BASE(public PlatformEventObserver<ListenerType>) {
 public:
  // Default delay between subsequent firing of events.
  static const int kDefaultPumpDelayMillis = 50;

  // PlatformEventObserver
  virtual void Start(blink::WebPlatformEventListener* listener) OVERRIDE {
    DVLOG(2) << "requested start";

    if (state_ != STOPPED)
      return;

    DCHECK(!timer_.IsRunning());

    PlatformEventObserver<ListenerType>::Start(listener);
    state_ = PENDING_START;
  }

  virtual void Stop() OVERRIDE {
    DVLOG(2) << "stop";

    if (state_ == STOPPED)
      return;

    DCHECK((state_ == PENDING_START && !timer_.IsRunning()) ||
        (state_ == RUNNING && timer_.IsRunning()));

    if (timer_.IsRunning())
      timer_.Stop();
    PlatformEventObserver<ListenerType>::Stop();
    state_ = STOPPED;
  }

 protected:
  explicit DeviceSensorEventPump(RenderThread* thread)
      : PlatformEventObserver<ListenerType>(thread),
        pump_delay_millis_(kDefaultPumpDelayMillis),
        state_(STOPPED) {
  }

  virtual ~DeviceSensorEventPump() {
  }

  // The pump is a tri-state automaton with allowed transitions as follows:
  // STOPPED -> PENDING_START
  // PENDING_START -> RUNNING
  // PENDING_START -> STOPPED
  // RUNNING -> STOPPED
  enum PumpState {
      STOPPED,
      RUNNING,
      PENDING_START
  };

  void OnDidStart(base::SharedMemoryHandle handle) {
    DVLOG(2) << "did start sensor event pump";

    if (state_ != PENDING_START)
      return;

    DCHECK(!timer_.IsRunning());

    if (InitializeReader(handle)) {
      timer_.Start(FROM_HERE,
                   base::TimeDelta::FromMilliseconds(pump_delay_millis_),
                   this, &DeviceSensorEventPump::FireEvent);
      state_ = RUNNING;
    }
  }

  virtual void FireEvent() = 0;
  virtual bool InitializeReader(base::SharedMemoryHandle handle) = 0;

  int pump_delay_millis_;
  PumpState state_;
  base::RepeatingTimer<DeviceSensorEventPump> timer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSensorEventPump);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_SENSOR_EVENT_PUMP_H_
