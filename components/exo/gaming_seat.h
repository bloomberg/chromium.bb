// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_GAMING_SEAT_H_
#define COMPONENTS_EXO_GAMING_SEAT_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/exo/wm_helper.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "ui/aura/client/focus_change_observer.h"

#if defined(USE_OZONE_GAMEPAD)
#include "ui/events/ozone/gamepad/gamepad_observer.h"
#endif

namespace exo {
class GamingSeatDelegate;
class GamepadDelegate;

using CreateGamepadDataFetcherCallback =
    base::Callback<std::unique_ptr<device::GamepadDataFetcher>()>;

// TODO(jkwang): always use ozone_gamepad when ozone is default for all Chrome
// OS builds. https://crbug.com/717246
// This class represents one gaming seat. It uses /device/gamepad or
// ozone/gamepad as backend and notifies corresponding GamepadDelegate of any
// gamepad changes.
class GamingSeat : public WMHelper::FocusObserver
#if defined(USE_OZONE_GAMEPAD)
                   ,
                   public ui::GamepadObserver
#endif
{
 public:
  // This class will monitor gamepad connection changes and manage gamepad
  // returned by gaming_seat_delegate.
  GamingSeat(GamingSeatDelegate* gaming_seat_delegate,
             base::SingleThreadTaskRunner* polling_task_runner);

  ~GamingSeat() override;

  // Overridden from WMHelper::FocusObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

#if defined(USE_OZONE_GAMEPAD)
  // Overridden from ui::GamepadObserver:
  void OnGamepadDevicesUpdated() override;
  void OnGamepadEvent(const ui::GamepadEvent& event) override;
#endif

 private:
  // The delegate that handles gamepad_added.
  GamingSeatDelegate* const delegate_;

#if defined(USE_OZONE_GAMEPAD)
  // Contains the delegate for each gamepad device.
  base::flat_map<int, GamepadDelegate*> gamepads_;

  // The flag if a valid target for gaming seat is focused. In other words, if
  // it's true, this class is observing gamepad events.
  bool focused_ = false;
#else
  class ThreadSafeGamepadChangeFetcher;

  // Processes updates of gamepad data and passes changes on to delegate.
  void ProcessGamepadChanges(int index, const device::Gamepad new_pad);

  // Private implementation of methods and resources that are used on the
  // polling thread.
  scoped_refptr<ThreadSafeGamepadChangeFetcher> gamepad_change_fetcher_;

  // The delegate instances that all other events are dispatched to.
  GamepadDelegate* gamepad_delegates_[device::Gamepads::kItemsLengthCap];

  // The current state of the gamepad represented by this instance.
  device::Gamepads pad_state_;

  // ThreadChecker for the origin thread.
  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<GamingSeat> weak_ptr_factory_;
#endif

  DISALLOW_COPY_AND_ASSIGN(GamingSeat);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_GAMING_SEAT_H_
