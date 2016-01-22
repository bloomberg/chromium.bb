// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_PROVIDER_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_PROVIDER_H_

#include <stdint.h>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/synchronization/lock.h"
#include "base/system_monitor/system_monitor.h"
#include "content/browser/gamepad/gamepad_standard_mappings.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace base {
class SingleThreadTaskRunner;
class Thread;
}

namespace content {

class GamepadDataFetcher;
struct GamepadHardwareBuffer;

enum GamepadSource {
  GAMEPAD_SOURCE_NONE = 0,
  GAMEPAD_SOURCE_ANDROID,
  GAMEPAD_SOURCE_LINUX_UDEV,
  GAMEPAD_SOURCE_MAC_HID,
  GAMEPAD_SOURCE_MAC_XBOX,
  GAMEPAD_SOURCE_TEST,
  GAMEPAD_SOURCE_WIN_XINPUT,
  GAMEPAD_SOURCE_WIN_RAW,
};

enum GamepadActiveState {
  GAMEPAD_INACTIVE = 0,
  GAMEPAD_ACTIVE,
  GAMEPAD_NEWLY_ACTIVE,
};

struct PadState {
  // Which data fetcher provided this gamepad's data.
  GamepadSource source;

  // Data fetcher-specific identifier for this gamepad.
  int source_id;

  // Indicates whether or not the gamepad is actively being updated
  GamepadActiveState active_state;

  // Gamepad data, unmapped.
  blink::WebGamepad data;

  // Functions to map from device data to standard layout, if available. May
  // be null if no mapping is available or needed.
  GamepadStandardMappingFunction mapper;

  // Sanitization masks
  // axis_mask and button_mask are bitfields that represent the reset state of
  // each input. If a button or axis has ever reported 0 in the past the
  // corresponding bit will be set to 1.

  // If we ever increase the max axis count this will need to be updated.
  static_assert(blink::WebGamepad::axesLengthCap <=
      std::numeric_limits<uint32_t>::digits,
      "axis_mask is not large enough");
  uint32_t axis_mask;

  // If we ever increase the max button count this will need to be updated.
  static_assert(blink::WebGamepad::buttonsLengthCap <=
      std::numeric_limits<uint32_t>::digits,
      "button_mask is not large enough");
  uint32_t button_mask;
};

class CONTENT_EXPORT GamepadProvider :
  public base::SystemMonitor::DevicesChangedObserver {
 public:
  GamepadProvider();

  // Manually specifies the data fetcher. Used for testing.
  explicit GamepadProvider(scoped_ptr<GamepadDataFetcher> fetcher);

  ~GamepadProvider() override;

  // Returns the shared memory handle of the gamepad data duplicated into the
  // given process.
  base::SharedMemoryHandle GetSharedMemoryHandleForProcess(
      base::ProcessHandle renderer_process);

  void GetCurrentGamepadData(blink::WebGamepads* data);

  // Pause and resume the background polling thread. Can be called from any
  // thread.
  void Pause();
  void Resume();

  // Registers the given closure for calling when the user has interacted with
  // the device. This callback will only be issued once.
  void RegisterForUserGesture(const base::Closure& closure);

  // base::SystemMonitor::DevicesChangedObserver implementation.
  void OnDevicesChanged(base::SystemMonitor::DeviceType type) override;

  // Add a gamepad data fetcher. Takes ownership of |fetcher|.
  void AddGamepadDataFetcher(scoped_ptr<GamepadDataFetcher> fetcher);

  // Gets a PadState object for the given source and id. If the device hasn't
  // been encountered before one of the remaining slots will be reserved for it.
  // If no slots are available will return NULL.
  PadState* GetPadState(GamepadSource source, int source_id);

  void SetSanitizationEnabled(bool sanitize) { sanitize_ = sanitize; }

 private:
  void Initialize(scoped_ptr<GamepadDataFetcher> fetcher);

  void DoAddGamepadDataFetcher(scoped_ptr<GamepadDataFetcher> fetcher);

  // Method for sending pause hints to the low-level data fetcher. Runs on
  // polling_thread_.
  void SendPauseHint(bool paused);

  // Method for polling a GamepadDataFetcher. Runs on the polling_thread_.
  void DoPoll();
  void ScheduleDoPoll();

  void OnGamepadConnectionChange(bool connected,
                                 int index,
                                 const blink::WebGamepad& pad);
  void DispatchGamepadConnectionChange(bool connected,
                                       int index,
                                       const blink::WebGamepad& pad);

  GamepadHardwareBuffer* SharedMemoryAsHardwareBuffer();

  // Checks the gamepad state to see if the user has interacted with it.
  void CheckForUserGesture();
  void ClearPadState(PadState& state);

  void MapAndSanitizeGamepadData(PadState* pad_state,
                                 blink::WebGamepad* pad);

  enum { kDesiredSamplingIntervalMs = 16 };

  // Keeps track of when the background thread is paused. Access to is_paused_
  // must be guarded by is_paused_lock_.
  base::Lock is_paused_lock_;
  bool is_paused_;

  // Keep track of when a polling task is scheduled, so as to prevent us from
  // accidentally scheduling more than one at any time, when rapidly toggling
  // |is_paused_|.
  bool have_scheduled_do_poll_;

  // Lists all observers registered for user gestures, and the thread which
  // to issue the callbacks on. Since we always issue the callback on the
  // thread which the registration happened, and this class lives on the I/O
  // thread, the message loop proxies will normally just be the I/O thread.
  // However, this will be the main thread for unit testing.
  base::Lock user_gesture_lock_;
  struct ClosureAndThread {
    ClosureAndThread(const base::Closure& c,
                     const scoped_refptr<base::SingleThreadTaskRunner>& m);
    ~ClosureAndThread();

    base::Closure closure;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  };
  typedef std::vector<ClosureAndThread> UserGestureObserverVector;
  UserGestureObserverVector user_gesture_observers_;

  // Updated based on notification from SystemMonitor when the system devices
  // have been updated, and this notification is passed on to the data fetcher
  // to enable it to avoid redundant (and possibly expensive) is-connected
  // tests. Access to devices_changed_ must be guarded by
  // devices_changed_lock_.
  base::Lock devices_changed_lock_;
  bool devices_changed_;

  bool ever_had_user_gesture_;
  bool sanitize_;

  // Tracks the state of each gamepad slot.
  scoped_ptr<PadState[]> pad_states_;

  // Only used on the polling thread.
  typedef std::vector<scoped_ptr<GamepadDataFetcher>> GamepadFetcherVector;
  GamepadFetcherVector data_fetchers_;

  base::Lock shared_memory_lock_;
  base::SharedMemory gamepad_shared_memory_;

  // Polling is done on this background thread.
  scoped_ptr<base::Thread> polling_thread_;

  DISALLOW_COPY_AND_ASSIGN(GamepadProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_PROVIDER_H_
