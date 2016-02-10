// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/input/arc_input_bridge.h"

#include <linux/input.h>
#include <fcntl.h>
#include <stddef.h>

#include <string>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/arc/arc_bridge_service.h"
#include "mojo/edk/embedder/embedder.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace {

// ARC runs as 32-bit in all platforms, so we need to make sure to send a
// struct input_event that is the same size it expects.
struct timeval32 {
  int32_t tv_sec;
  int32_t tv_usec;
};

struct input_event32 {
  struct timeval32 time;
  uint16_t type;
  uint16_t code;
  int32_t value;
};

// input_event values for keyboard events.
const int kKeyReleased = 0;
const int kKeyPressed = 1;
const int kKeyRepeated = 2;

// maximum number of supported multi-touch slots (simultaneous fingers).
const int kMaxSlots = 64;

// tracking id of an empty slot.
const int kEmptySlot = -1;

// maximum possible pressure as defined in EventHubARC.
// TODO(denniskempin): communicate maximum during initialization.
const int kMaxPressure = 65536;

// speed of the scroll emulation. Scroll events are reported at about 100 times
// the speed of a scroll wheel.
const float kScrollEmulationSpeed = 100.0f;

struct MouseButtonMapping {
  int ui_flag;
  int evdev_code;
} kMouseButtonMap[] = {
    {ui::EF_LEFT_MOUSE_BUTTON, BTN_LEFT},
    {ui::EF_RIGHT_MOUSE_BUTTON, BTN_RIGHT},
    {ui::EF_MIDDLE_MOUSE_BUTTON, BTN_MIDDLE},
};

// Offset between evdev key codes and chrome native key codes
const int kXkbKeycodeOffset = 8;

}  // namespace

namespace arc {

ArcInputBridge::ArcInputBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service),
      offset_x_acc_(0.5f),
      offset_y_acc_(0.5f),
      current_slot_(-1),
      current_slot_tracking_ids_(kMaxSlots, kEmptySlot),
      origin_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
  arc_bridge_service()->AddObserver(this);

  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->AddObserver(this);
}

ArcInputBridge::~ArcInputBridge() {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  arc_bridge_service()->RemoveObserver(this);

  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->RemoveObserver(this);

  for (aura::Window* window : arc_windows_.windows()) {
    window->RemovePreTargetHandler(this);
  }
}

void ArcInputBridge::OnInputInstanceReady() {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());

  keyboard_fd_ = CreateBridgeInputDevice("ChromeOS Keyboard", "keyboard");
  mouse_fd_ = CreateBridgeInputDevice("ChromeOS Mouse", "mouse");
  touchscreen_fd_ =
      CreateBridgeInputDevice("ChromeOS Touchscreen", "touchscreen");
}

// Translates and sends a ui::Event to the appropriate bridge device of the
// ARC instance. If the devices have not yet been initialized, the event
// will be ignored.
void ArcInputBridge::OnEvent(ui::Event* event) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  if (event->IsKeyEvent()) {
    SendKeyEvent(static_cast<ui::KeyEvent*>(event));
  } else if (event->IsMouseEvent() || event->IsScrollEvent()) {
    SendMouseEvent(static_cast<ui::MouseEvent*>(event));
  } else if (event->IsTouchEvent()) {
    SendTouchEvent(static_cast<ui::TouchEvent*>(event));
  }
}

// Attaches the input bridge to the window if it is marked as an ARC window.
void ArcInputBridge::OnWindowInitialized(aura::Window* new_window) {
  if (new_window->name() == "ExoSurface") {
    arc_windows_.Add(new_window);
    new_window->AddPreTargetHandler(this);
  }
}

void ArcInputBridge::SendKeyEvent(ui::KeyEvent* event) {
  if (keyboard_fd_.get() < 0) {
    VLOG(2) << "No keyboard bridge device available.";
    return;
  }

  uint16_t evdev_code = DomCodeToEvdevCode(event->code());
  int evdev_value = 0;
  if (event->type() == ui::ET_KEY_PRESSED) {
    if (event->flags() & ui::EF_IS_REPEAT) {
      evdev_value = kKeyRepeated;
    } else {
      evdev_value = kKeyPressed;
    }
  } else if (event->type() == ui::ET_KEY_RELEASED) {
    evdev_value = kKeyReleased;
  } else {
    NOTREACHED() << "Key should be either PRESSED or RELEASED.";
  }

  base::TimeDelta time_stamp = event->time_stamp();
  SendKernelEvent(keyboard_fd_, time_stamp, EV_KEY, evdev_code, evdev_value);
  SendSynReport(keyboard_fd_, time_stamp);
}

void ArcInputBridge::SendTouchEvent(ui::TouchEvent* event) {
  if (touchscreen_fd_.get() < 0) {
    VLOG(2) << "No touchscreen bridge device available.";
    return;
  }

  ui::PointerDetails details = event->pointer_details();
  base::TimeDelta time_stamp = event->time_stamp();

  // find or assing a slot for this tracking id
  int slot_id = AcquireTouchSlot(event);
  if (slot_id < 0) {
    VLOG(1) << "Ran out of slot IDs.";
    return;
  }

  // we only need to send the slot ID when it has changed.
  if (slot_id != current_slot_) {
    current_slot_ = slot_id;
    SendKernelEvent(touchscreen_fd_, time_stamp, EV_ABS, ABS_MT_SLOT,
                    current_slot_);
  }

  // update tracking id
  if (event->type() == ui::ET_TOUCH_PRESSED) {
    SendKernelEvent(touchscreen_fd_, time_stamp, EV_ABS, ABS_MT_TRACKING_ID,
                    event->touch_id());
  } else if (event->type() == ui::ET_TOUCH_RELEASED) {
    SendKernelEvent(touchscreen_fd_, time_stamp, EV_ABS, ABS_MT_TRACKING_ID,
                    kEmptySlot);
  }

  // update touch information
  if (event->type() == ui::ET_TOUCH_MOVED ||
      event->type() == ui::ET_TOUCH_PRESSED) {
    SendKernelEvent(touchscreen_fd_, time_stamp, EV_ABS, ABS_MT_POSITION_X,
                    event->x());
    SendKernelEvent(touchscreen_fd_, time_stamp, EV_ABS, ABS_MT_POSITION_Y,
                    event->y());
    SendKernelEvent(touchscreen_fd_, time_stamp, EV_ABS, ABS_MT_TOUCH_MAJOR,
                    details.radius_x);
    SendKernelEvent(touchscreen_fd_, time_stamp, EV_ABS, ABS_MT_TOUCH_MINOR,
                    details.radius_y);
    SendKernelEvent(touchscreen_fd_, time_stamp, EV_ABS, ABS_MT_PRESSURE,
                    details.force * kMaxPressure);
  }
  SendSynReport(touchscreen_fd_, time_stamp);
}

void ArcInputBridge::SendMouseEvent(ui::MouseEvent* event) {
  if (mouse_fd_.get() < 0) {
    VLOG(2) << "No mouse bridge device available.";
    return;
  }

  base::TimeDelta time_stamp = event->time_stamp();

  // update location
  if (event->type() == ui::ET_MOUSE_MOVED ||
      event->type() == ui::ET_MOUSE_DRAGGED) {
    SendKernelEvent(mouse_fd_, time_stamp, EV_ABS, ABS_X, event->x());
    SendKernelEvent(mouse_fd_, time_stamp, EV_ABS, ABS_Y, event->y());
  }

  // update buttons
  if (event->type() == ui::ET_MOUSE_PRESSED ||
      event->type() == ui::ET_MOUSE_RELEASED) {
    int evdev_value = static_cast<int>(event->type() == ui::ET_MOUSE_PRESSED);
    for (MouseButtonMapping mapping : kMouseButtonMap) {
      if (event->changed_button_flags() & mapping.ui_flag) {
        SendKernelEvent(mouse_fd_, time_stamp, EV_KEY, mapping.evdev_code,
                        evdev_value);
      }
    }
  }

  // update scroll wheel
  if (event->type() == ui::ET_SCROLL) {
    ui::ScrollEvent* scroll_event = static_cast<ui::ScrollEvent*>(event);
    // accumulate floating point scroll offset since we can only send full
    // integer
    // wheel events.
    offset_x_acc_ += scroll_event->x_offset_ordinal() / kScrollEmulationSpeed;
    offset_y_acc_ += scroll_event->y_offset_ordinal() / kScrollEmulationSpeed;

    int wheel = floor(offset_y_acc_);
    if (wheel != 0) {
      SendKernelEvent(mouse_fd_, time_stamp, EV_REL, REL_WHEEL, wheel);
      offset_y_acc_ -= static_cast<float>(wheel);
    }

    int hwheel = floor(offset_x_acc_);
    if (hwheel != 0) {
      SendKernelEvent(mouse_fd_, time_stamp, EV_REL, REL_HWHEEL, hwheel);
      offset_x_acc_ -= static_cast<float>(hwheel);
    }
  }

  SendSynReport(mouse_fd_, time_stamp);
}

void ArcInputBridge::SendKernelEvent(const base::ScopedFD& fd,
                                     base::TimeDelta time_stamp,
                                     uint16_t type,
                                     uint16_t code,
                                     int value) {
  DCHECK(fd.is_valid());

  struct input_event32 event;
  event.time.tv_sec = time_stamp.InSeconds();
  base::TimeDelta remainder =
      time_stamp - base::TimeDelta::FromSeconds(event.time.tv_sec);
  event.time.tv_usec = remainder.InMicroseconds();
  event.type = type;
  event.code = code;
  event.value = value;

  // Write event to file descriptor
  size_t num_written = write(fd.get(), reinterpret_cast<void*>(&event),
                             sizeof(struct input_event32));
  DCHECK_EQ(num_written, sizeof(struct input_event32));
}

void ArcInputBridge::SendSynReport(const base::ScopedFD& fd,
                                   base::TimeDelta time) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());

  SendKernelEvent(fd, time, EV_SYN, SYN_REPORT, 0);
}

int ArcInputBridge::AcquireTouchSlot(ui::TouchEvent* event) {
  int slot_id;
  if (event->type() == ui::ET_TOUCH_PRESSED) {
    slot_id = FindTouchSlot(kEmptySlot);
  } else {
    slot_id = FindTouchSlot(event->touch_id());
  }
  if (slot_id < 0) {
    return -1;
  }

  if (event->type() == ui::ET_TOUCH_RELEASED) {
    current_slot_tracking_ids_[slot_id] = kEmptySlot;
  } else if (event->type() == ui::ET_TOUCH_PRESSED) {
    current_slot_tracking_ids_[slot_id] = event->touch_id();
  }
  return slot_id;
}

int ArcInputBridge::FindTouchSlot(int tracking_id) {
  for (int i = 0; i < kMaxSlots; ++i) {
    if (current_slot_tracking_ids_[i] == tracking_id) {
      return i;
    }
  }
  return -1;
}

uint16_t ArcInputBridge::DomCodeToEvdevCode(ui::DomCode dom_code) {
  int native_code = ui::KeycodeConverter::DomCodeToNativeKeycode(dom_code);
  if (native_code == ui::KeycodeConverter::InvalidNativeKeycode())
    return KEY_RESERVED;

  return native_code - kXkbKeycodeOffset;
}

base::ScopedFD ArcInputBridge::CreateBridgeInputDevice(
    const std::string& name,
    const std::string& device_type) {
  // Create file descriptor pair for communication
  int fd[2];
  int res = HANDLE_EINTR(pipe(fd));
  if (res < 0) {
    VPLOG(1) << "Cannot create pipe";
    return base::ScopedFD();
  }
  base::ScopedFD read_fd(fd[0]);
  base::ScopedFD write_fd(fd[1]);

  // The read end is sent to the instance, ownership of fd transfers.
  InputInstance* input_instance = arc_bridge_service()->input_instance();
  if (!input_instance) {
    VLOG(1) << "ArcBridgeService InputInstance disappeared.";
    return base::ScopedFD();
  }
  MojoHandle wrapped_handle;
  MojoResult wrap_result = mojo::edk::CreatePlatformHandleWrapper(
      mojo::edk::ScopedPlatformHandle(
          mojo::edk::PlatformHandle(read_fd.release())),
      &wrapped_handle);
  if (wrap_result != MOJO_RESULT_OK) {
    LOG(WARNING) << "Pipe failed to wrap handles. Closing: " << wrap_result;
    return base::ScopedFD();
  }
  input_instance->RegisterInputDevice(
      name, device_type, mojo::ScopedHandle(mojo::Handle(wrapped_handle)));

  // setup write end as non blocking
  int flags = HANDLE_EINTR(fcntl(write_fd.get(), F_GETFL, 0));
  if (res < 0) {
    VPLOG(1) << "Cannot get file descriptor flags";
    return base::ScopedFD();
  }

  res = HANDLE_EINTR(fcntl(write_fd.get(), F_SETFL, flags | O_NONBLOCK));
  if (res < 0) {
    VPLOG(1) << "Cannot set file descriptor flags";
    return base::ScopedFD();
  }
  return write_fd;
}

}  // namespace arc
