// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_ARC_INPUT_ARC_INPUT_BRIDGE_IMPL_H_
#define COMPONENTS_EXO_ARC_INPUT_ARC_INPUT_BRIDGE_IMPL_H_

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "components/arc/input/arc_input_bridge.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"

namespace aura {
class Window;
}

namespace ui {
enum class DomCode;
}

namespace arc {

class ArcBridgeService;

// Private implementation of ArcInputBridge
class ArcInputBridgeImpl : public ArcInputBridge,
                           public ArcBridgeService::Observer,
                           public aura::EnvObserver,
                           public ui::EventHandler {
 public:
  // The constructor will register an Observer with aura::Env and the
  // ArcBridgeService. From then on, no further interaction with this class
  // is needed.
  explicit ArcInputBridgeImpl(ArcBridgeService* arc_bridge_service);
  ~ArcInputBridgeImpl() override;

  // Overridden from ui::EventHandler:
  void OnEvent(ui::Event* event) override;

  // Overridden from aura::EnvObserver:
  void OnWindowInitialized(aura::Window* new_window) override;

  // Overridden from ArcBridgeService::Observer:
  void OnInstanceBootPhase(InstanceBootPhase phase) override;

 private:
  // Specialized method to translate and send events to the right file
  // descriptor.
  void SendKeyEvent(ui::KeyEvent* event);
  void SendTouchEvent(ui::TouchEvent* event);
  void SendMouseEvent(ui::MouseEvent* event);

  // Helper method to send a struct input_event to the file descriptor. This
  // method is to be called on the ui thread and will post a request to send
  // the event to the io thread.
  // The parameters map directly to the members of input_event as
  // defined by the evdev protocol.
  // |type| is the type of event to sent, such as EV_SYN, EV_KEY, EV_ABS.
  // |code| is either interpreted as axis (ABS_X, ABS_Y, ...) or as key-code
  //        (KEY_A, KEY_B, ...).
  // |value| is either the value of that axis or the boolean value of the key
  //         as in 0 (released), 1 (pressed) or 2 (repeated press).
  void SendKernelEvent(const base::ScopedFD& fd,
                       base::TimeDelta timestamp,
                       unsigned short type,
                       unsigned short code,
                       int value);

  // Shorthand for sending EV_SYN/SYN_REPORT
  void SendSynReport(const base::ScopedFD& fd, base::TimeDelta timestamp);

  // Return existing or new slot for this event.
  int AcquireTouchSlot(ui::TouchEvent* event);

  // Return touch slot for tracking id.
  int FindTouchSlot(int tracking_id);

  // Maps DOM key codes to evdev key codes
  unsigned short DomCodeToEvdevCode(ui::DomCode dom_code);


  // Setup bridge devices on the instance side. This needs to be called after
  // the InstanceBootPhase::SYSTEM_SERVICES_READY has been reached.
  void SetupBridgeDevices();

  // Creates and registers file descriptor pair with the ARC bridge service,
  // the write end is returned while the read end is sent through the bridge
  // to the ARC instance.
  // TODO(denniskempin): Make this interface more typesafe.
  // |name| should be the displayable name of the emulated device (e.g. "Chrome
  // OS Keyboard"), |device_type| the name of the device type (e.g. "keyboard")
  // and |fd| a file descriptor that emulates the kernel events of the device.
  // This can only be called on the thread that this class was created on.
  base::ScopedFD CreateBridgeInputDevice(const std::string& name,
                                         const std::string& device_type);

  // Owned by ArcServiceManager which makes sure ArcBridgeService is destroyed
  // after ArcInputBridge.
  ArcBridgeService* arc_bridge_service_;

  // File descriptors for the different device types.
  base::ScopedFD keyboard_fd_;
  base::ScopedFD mouse_fd_;
  base::ScopedFD touchscreen_fd_;

  // Scroll accumlator.
  float offset_x_acc_;
  float offset_y_acc_;

  // Currently selected slot for multi-touch events.
  int current_slot_;

  // List of touch tracking id to slot assignments.
  std::vector<int> current_slot_tracking_ids_;

  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;

  // List of windows we are hooked into
  aura::WindowTracker arc_windows_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcInputBridgeImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcInputBridgeImpl);
};

}  // namespace arc

#endif  // COMPONENTS_EXO_ARC_INPUT_ARC_INPUT_BRIDGE_IMPL_H_
