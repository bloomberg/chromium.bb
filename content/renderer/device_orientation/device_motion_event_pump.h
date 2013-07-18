// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVICE_MOTION_EVENT_PUMP_H_
#define CONTENT_RENDERER_DEVICE_MOTION_EVENT_PUMP_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/timer/timer.h"
#include "content/public/renderer/render_process_observer.h"
#include "content/renderer/shared_memory_seqlock_reader.h"
#include "third_party/WebKit/public/platform/WebDeviceMotionData.h"

namespace WebKit {
class WebDeviceMotionListener;
}

namespace content {
class RenderThread;

typedef SharedMemorySeqLockReader<WebKit::WebDeviceMotionData>
    DeviceMotionSharedMemoryReader;

class CONTENT_EXPORT DeviceMotionEventPump : public RenderProcessObserver {
 public:
  DeviceMotionEventPump();
  virtual ~DeviceMotionEventPump();

  static double GetDelayMillis();

  // Sets the listener to receive updates for device motion data at
  // regular intervals.
  // Returns true if the registration was successful.
  bool SetListener(WebKit::WebDeviceMotionListener*);

  void SetDeviceMotionReader(scoped_ptr<DeviceMotionSharedMemoryReader>);

  void Attach(RenderThread* thread);

  // RenderProcessObserver implementation.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

 protected:
  // Delay between subsequent firing of events.
  static const double kPumpDelayMillis;

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

  bool StartFetchingDeviceMotion();
  bool StopFetchingDeviceMotion();
  void OnDidStartDeviceMotion(base::SharedMemoryHandle renderer_handle);
  void FireEvent();

  WebKit::WebDeviceMotionListener* listener_;
  scoped_ptr<DeviceMotionSharedMemoryReader> reader_;
  base::RepeatingTimer<DeviceMotionEventPump> timer_;
  PumpState state_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVICE_MOTION_EVENT_PUMP_H_
