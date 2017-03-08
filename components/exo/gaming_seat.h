// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_GAMING_SEAT_H_
#define COMPONENTS_EXO_GAMING_SEAT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/exo/wm_helper.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "ui/aura/client/focus_change_observer.h"

namespace exo {
class GamingSeatDelegate;
class GamepadDelegate;

using CreateGamepadDataFetcherCallback =
    base::Callback<std::unique_ptr<device::GamepadDataFetcher>()>;

// This class represents one gaming seat, it uses a background thread
// for polling gamepad devices and notifies the corresponding GampadDelegate of
// any changes.
class GamingSeat : public WMHelper::FocusObserver {
 public:
  // This class will post tasks to invoke the delegate on the thread runner
  // which is associated with the thread that is creating this instance.
  GamingSeat(GamingSeatDelegate* gaming_seat_delegate,
             base::SingleThreadTaskRunner* polling_task_runner);

  // Allows test cases to specify a CreateGamepadDataFetcherCallback that
  // overrides the default GamepadPlatformDataFetcher.
  GamingSeat(GamingSeatDelegate* gaming_seat_delegate,
             base::SingleThreadTaskRunner* polling_task_runner,
             CreateGamepadDataFetcherCallback create_fetcher_callback);

  ~GamingSeat() override;

  // Overridden WMHelper::FocusObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

 private:
  class ThreadSafeGamepadChangeFetcher;

  // Processes updates of gamepad data and passes changes on to delegate.
  void ProcessGamepadChanges(int index, const blink::WebGamepad new_pad);

  // Private implementation of methods and resources that are used on the
  // polling thread.
  scoped_refptr<ThreadSafeGamepadChangeFetcher> gamepad_change_fetcher_;

  // The delegate that handles gamepad_added.
  GamingSeatDelegate* const delegate_;

  // The delegate instances that all other events are dispatched to.
  GamepadDelegate* gamepad_delegates_[blink::WebGamepads::itemsLengthCap];

  // The current state of the gamepad represented by this instance.
  blink::WebGamepads pad_state_;

  // ThreadChecker for the origin thread.
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<GamingSeat> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GamingSeat);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_GAMING_SEAT_H_
