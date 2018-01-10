// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/user_input_monitor.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/win/message_window.h"
#include "media/base/keyboard_event_counter.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/events/keycodes/keyboard_code_conversion_win.h"

namespace media {
namespace {

// From the HID Usage Tables specification.
const USHORT kGenericDesktopPage = 1;
const USHORT kKeyboardUsage = 6;

std::unique_ptr<RAWINPUTDEVICE> GetRawInputDevices(HWND hwnd, DWORD flags) {
  std::unique_ptr<RAWINPUTDEVICE> device(new RAWINPUTDEVICE());
  device->dwFlags = flags;
  device->usUsagePage = kGenericDesktopPage;
  device->usUsage = kKeyboardUsage;
  device->hwndTarget = hwnd;
  return device;
}

// This is the actual implementation of event monitoring. It's separated from
// UserInputMonitorWin since it needs to be deleted on the UI thread.
class UserInputMonitorWinCore
    : public base::SupportsWeakPtr<UserInputMonitorWinCore>,
      public base::MessageLoop::DestructionObserver {
 public:
  enum EventBitMask {
    MOUSE_EVENT_MASK = 1,
    KEYBOARD_EVENT_MASK = 2,
  };

  explicit UserInputMonitorWinCore(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  ~UserInputMonitorWinCore() override;

  // DestructionObserver overrides.
  void WillDestroyCurrentMessageLoop() override;

  size_t GetKeyPressCount() const;
  void StartMonitor();
  void StopMonitor();

 private:
  // Handles WM_INPUT messages.
  LRESULT OnInput(HRAWINPUT input_handle);
  // Handles messages received by |window_|.
  bool HandleMessage(UINT message,
                     WPARAM wparam,
                     LPARAM lparam,
                     LRESULT* result);

  // Task runner on which |window_| is created.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // These members are only accessed on the UI thread.
  std::unique_ptr<base::win::MessageWindow> window_;
  KeyboardEventCounter counter_;

  DISALLOW_COPY_AND_ASSIGN(UserInputMonitorWinCore);
};

class UserInputMonitorWin : public UserInputMonitor {
 public:
  explicit UserInputMonitorWin(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner);
  ~UserInputMonitorWin() override;

  // Public UserInputMonitor overrides.
  size_t GetKeyPressCount() const override;

 private:
  // Private UserInputMonitor overrides.
  void StartKeyboardMonitoring() override;
  void StopKeyboardMonitoring() override;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  UserInputMonitorWinCore* core_;

  DISALLOW_COPY_AND_ASSIGN(UserInputMonitorWin);
};

UserInputMonitorWinCore::UserInputMonitorWinCore(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : ui_task_runner_(ui_task_runner) {}

UserInputMonitorWinCore::~UserInputMonitorWinCore() {
  DCHECK(!window_);
}

void UserInputMonitorWinCore::WillDestroyCurrentMessageLoop() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  StopMonitor();
}

size_t UserInputMonitorWinCore::GetKeyPressCount() const {
  return counter_.GetKeyPressCount();
}

void UserInputMonitorWinCore::StartMonitor() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (window_)
    return;

  std::unique_ptr<base::win::MessageWindow> window =
      std::make_unique<base::win::MessageWindow>();
  if (!window->Create(base::Bind(&UserInputMonitorWinCore::HandleMessage,
                                 base::Unretained(this)))) {
    PLOG(ERROR) << "Failed to create the raw input window";
    return;
  }

  // Register to receive raw keyboard input.
  std::unique_ptr<RAWINPUTDEVICE> device(
      GetRawInputDevices(window->hwnd(), RIDEV_INPUTSINK));
  if (!RegisterRawInputDevices(device.get(), 1, sizeof(*device))) {
    PLOG(ERROR) << "RegisterRawInputDevices() failed for RIDEV_INPUTSINK";
    return;
  }

  window_ = std::move(window);
  // Start observing message loop destruction if we start monitoring the first
  // event.
  base::MessageLoop::current()->AddDestructionObserver(this);
}

void UserInputMonitorWinCore::StopMonitor() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (!window_)
    return;

  // Stop receiving raw input.
  std::unique_ptr<RAWINPUTDEVICE> device(
      GetRawInputDevices(window_->hwnd(), RIDEV_REMOVE));

  if (!RegisterRawInputDevices(device.get(), 1, sizeof(*device))) {
    PLOG(INFO) << "RegisterRawInputDevices() failed for RIDEV_REMOVE";
  }

  window_ = nullptr;

  // Stop observing message loop destruction if no event is being monitored.
  base::MessageLoop::current()->RemoveDestructionObserver(this);
}

LRESULT UserInputMonitorWinCore::OnInput(HRAWINPUT input_handle) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Get the size of the input record.
  UINT size = 0;
  UINT result = GetRawInputData(
      input_handle, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputData() failed";
    return 0;
  }
  DCHECK_EQ(0u, result);

  // Retrieve the input record itself.
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[size]);
  RAWINPUT* input = reinterpret_cast<RAWINPUT*>(buffer.get());
  result = GetRawInputData(
      input_handle, RID_INPUT, buffer.get(), &size, sizeof(RAWINPUTHEADER));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputData() failed";
    return 0;
  }
  DCHECK_EQ(size, result);

  // Notify the observer about events generated locally.
  if (input->header.dwType == RIM_TYPEKEYBOARD &&
      input->header.hDevice != NULL) {
    ui::EventType event = (input->data.keyboard.Flags & RI_KEY_BREAK)
                              ? ui::ET_KEY_RELEASED
                              : ui::ET_KEY_PRESSED;
    ui::KeyboardCode key_code =
        ui::KeyboardCodeForWindowsKeyCode(input->data.keyboard.VKey);
    counter_.OnKeyboardEvent(event, key_code);
  }

  return DefRawInputProc(&input, 1, sizeof(RAWINPUTHEADER));
}

bool UserInputMonitorWinCore::HandleMessage(UINT message,
                                            WPARAM wparam,
                                            LPARAM lparam,
                                            LRESULT* result) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  switch (message) {
    case WM_INPUT:
      *result = OnInput(reinterpret_cast<HRAWINPUT>(lparam));
      return true;

    default:
      return false;
  }
}

//
// Implementation of UserInputMonitorWin.
//

UserInputMonitorWin::UserInputMonitorWin(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
    : ui_task_runner_(ui_task_runner),
      core_(new UserInputMonitorWinCore(ui_task_runner)) {}

UserInputMonitorWin::~UserInputMonitorWin() {
  if (!ui_task_runner_->DeleteSoon(FROM_HERE, core_))
    delete core_;
}

size_t UserInputMonitorWin::GetKeyPressCount() const {
  return core_->GetKeyPressCount();
}

void UserInputMonitorWin::StartKeyboardMonitoring() {
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UserInputMonitorWinCore::StartMonitor, core_->AsWeakPtr()));
}

void UserInputMonitorWin::StopKeyboardMonitoring() {
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UserInputMonitorWinCore::StopMonitor, core_->AsWeakPtr()));
}

}  // namespace

std::unique_ptr<UserInputMonitor> UserInputMonitor::Create(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner) {
  return std::make_unique<UserInputMonitorWin>(ui_task_runner);
}

}  // namespace media
