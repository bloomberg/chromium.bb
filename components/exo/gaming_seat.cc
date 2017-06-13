// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/gaming_seat.h"

#include <cmath>

#include "base/bind.h"
#include "components/exo/gamepad_delegate.h"
#include "components/exo/gaming_seat_delegate.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_pad_state_provider.h"
#include "device/gamepad/gamepad_platform_data_fetcher_linux.h"
#include "ui/aura/window.h"

namespace exo {
namespace {

constexpr double kGamepadButtonValueEpsilon = 0.001;
bool GamepadButtonValuesAreEqual(double a, double b) {
  return fabs(a - b) < kGamepadButtonValueEpsilon;
}

std::unique_ptr<device::GamepadDataFetcher> CreateGamepadPlatformDataFetcher() {
  return std::unique_ptr<device::GamepadDataFetcher>(
      new device::GamepadPlatformDataFetcherLinux());
}

// Time between gamepad polls in milliseconds.
constexpr unsigned kPollingTimeIntervalMs = 16;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// GamingSeat::ThreadSafeGamepadChangeFetcher

// Implements all methods and resources running on the polling thread.
// This class is reference counted to allow it to shut down safely on the
// polling thread even if the Gamepad has been destroyed on the origin thread.
class GamingSeat::ThreadSafeGamepadChangeFetcher
    : public device::GamepadPadStateProvider,
      public base::RefCountedThreadSafe<
          GamingSeat::ThreadSafeGamepadChangeFetcher> {
 public:
  using ProcessGamepadChangesCallback =
      base::Callback<void(int index, const device::Gamepad)>;

  ThreadSafeGamepadChangeFetcher(
      const ProcessGamepadChangesCallback& post_gamepad_changes,
      const CreateGamepadDataFetcherCallback& create_fetcher_callback,
      base::SingleThreadTaskRunner* task_runner)
      : process_gamepad_changes_(post_gamepad_changes),
        create_fetcher_callback_(create_fetcher_callback),
        polling_task_runner_(task_runner),
        origin_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
    DETACH_FROM_THREAD(thread_checker_);
  }

  // Enable or disable gamepad polling. Can be called from any thread.
  void EnablePolling(bool enabled) {
    polling_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &ThreadSafeGamepadChangeFetcher::EnablePollingOnPollingThread,
            make_scoped_refptr(this), enabled));
  }

 private:
  friend class base::RefCountedThreadSafe<ThreadSafeGamepadChangeFetcher>;

  ~ThreadSafeGamepadChangeFetcher() override {}

  // Enables or disables polling.
  void EnablePollingOnPollingThread(bool enabled) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    is_enabled_ = enabled;

    if (is_enabled_) {
      if (!fetcher_) {
        fetcher_ = create_fetcher_callback_.Run();
        InitializeDataFetcher(fetcher_.get());
        DCHECK(fetcher_);
      }
      SchedulePollOnPollingThread();
    } else {
      fetcher_.reset();
    }
  }

  // Schedules the next poll on the polling thread.
  void SchedulePollOnPollingThread() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(fetcher_);

    if (!is_enabled_ || has_poll_scheduled_)
      return;

    has_poll_scheduled_ = true;
    polling_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ThreadSafeGamepadChangeFetcher::PollOnPollingThread,
                   make_scoped_refptr(this)),
        base::TimeDelta::FromMilliseconds(kPollingTimeIntervalMs));
  }

  // Polls devices for new data and posts gamepad changes back to origin thread.
  void PollOnPollingThread() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    has_poll_scheduled_ = false;
    if (!is_enabled_)
      return;

    DCHECK(fetcher_);

    device::Gamepads new_state = state_;
    fetcher_->GetGamepadData(
        false /* No hardware changed notification from the system */);

    for (size_t i = 0; i < device::Gamepads::kItemsLengthCap; ++i) {
      device::PadState& pad_state = pad_states_.get()[i];

      // After querying the gamepad clear the state if it did not have it's
      // active
      // state updated but is still listed as being associated with a specific
      // source. This indicates the gamepad is disconnected.
      if (!pad_state.active_state &&
          pad_state.source != device::GAMEPAD_SOURCE_NONE) {
        ClearPadState(pad_state);
      }

      MapAndSanitizeGamepadData(&pad_state, &new_state.items[i],
                                false /* Don't sanitize gamepad data */);

      // If the gamepad is still actively reporting the next call to
      // GetGamepadData will set the active state to active again.
      if (pad_state.active_state)
        pad_state.active_state = device::GAMEPAD_INACTIVE;

      if (new_state.items[i].connected != state_.items[i].connected ||
          new_state.items[i].timestamp > state_.items[i].timestamp) {
        origin_task_runner_->PostTask(
            FROM_HERE,
            base::Bind(process_gamepad_changes_, i, new_state.items[i]));
      }
    }
    state_ = new_state;
    SchedulePollOnPollingThread();
  }

  // Callback to ProcessGamepadChanges using weak reference to Gamepad.
  ProcessGamepadChangesCallback process_gamepad_changes_;

  // Callback method to create a gamepad data fetcher.
  CreateGamepadDataFetcherCallback create_fetcher_callback_;

  // Reference to task runner on polling thread.
  scoped_refptr<base::SingleThreadTaskRunner> polling_task_runner_;

  // Reference to task runner on origin thread.
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;

  // Gamepad data fetcher used for querying gamepad devices.
  std::unique_ptr<device::GamepadDataFetcher> fetcher_;

  // The current state of all gamepads.
  device::Gamepads state_;

  // True if a poll has been scheduled.
  bool has_poll_scheduled_ = false;

  // True if the polling thread is paused.
  bool is_enabled_ = false;

  // ThreadChecker for the polling thread.
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(ThreadSafeGamepadChangeFetcher);
};

////////////////////////////////////////////////////////////////////////////////
// GamingSeat, public:

GamingSeat::GamingSeat(GamingSeatDelegate* gaming_seat_delegate,
                       base::SingleThreadTaskRunner* polling_task_runner)
    : delegate_(gaming_seat_delegate),
      gamepad_delegates_{nullptr},
      weak_ptr_factory_(this) {
  gamepad_change_fetcher_ = new ThreadSafeGamepadChangeFetcher(
      base::Bind(&GamingSeat::ProcessGamepadChanges,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(CreateGamepadPlatformDataFetcher), polling_task_runner);

  auto* helper = WMHelper::GetInstance();
  helper->AddFocusObserver(this);
  OnWindowFocused(helper->GetFocusedWindow(), nullptr);
}

GamingSeat::~GamingSeat() {
  // Disable polling. Since ThreadSafeGamepadChangeFetcher are reference
  // counted, we can safely have it shut down after Gamepad has been destroyed.
  gamepad_change_fetcher_->EnablePolling(false);

  delegate_->OnGamingSeatDestroying(this);
  for (size_t i = 0; i < device::Gamepads::kItemsLengthCap; ++i) {
    if (gamepad_delegates_[i]) {
      gamepad_delegates_[i]->OnRemoved();
    }
  }
  WMHelper::GetInstance()->RemoveFocusObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// aura::client::FocusChangeObserver overrides:

void GamingSeat::OnWindowFocused(aura::Window* gained_focus,
                                 aura::Window* lost_focus) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Surface* target = nullptr;
  if (gained_focus) {
    target = Surface::AsSurface(gained_focus);
    if (!target) {
      aura::Window* top_level_window = gained_focus->GetToplevelWindow();
      if (top_level_window)
        target = ShellSurface::GetMainSurface(top_level_window);
    }
  }

  bool focused = target && delegate_->CanAcceptGamepadEventsForSurface(target);

  gamepad_change_fetcher_->EnablePolling(focused);
}

////////////////////////////////////////////////////////////////////////////////
// GamingSeat, private:

void GamingSeat::ProcessGamepadChanges(int index,
                                       const device::Gamepad new_pad) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool send_frame = false;

  device::Gamepad& pad_state = pad_state_.items[index];
  // Update connection state.
  GamepadDelegate* delegate = gamepad_delegates_[index];
  if (new_pad.connected != pad_state.connected) {
    // New pad is disconnected.
    if (!new_pad.connected) {
      // If gamepad is disconnected now, it should be connected before, then
      // gamepad_delegate should not be null.
      DCHECK(delegate);
      delegate->OnRemoved();
      gamepad_delegates_[index] = nullptr;
      pad_state = new_pad;
      return;
    } else if (new_pad.connected) {
      gamepad_delegates_[index] = delegate_->GamepadAdded();
    }
  }

  if (!delegate || !new_pad.connected ||
      new_pad.timestamp <= pad_state.timestamp) {
    pad_state = new_pad;
    return;
  }

  // Notify delegate of updated axes.
  for (size_t axis = 0;
       axis < std::max(pad_state.axes_length, new_pad.axes_length); ++axis) {
    if (!GamepadButtonValuesAreEqual(new_pad.axes[axis],
                                     pad_state.axes[axis])) {
      send_frame = true;
      delegate->OnAxis(axis, new_pad.axes[axis]);
    }
  }

  // Notify delegate of updated buttons.
  for (size_t button_id = 0;
       button_id < std::max(pad_state.buttons_length, new_pad.buttons_length);
       ++button_id) {
    auto button = pad_state.buttons[button_id];
    auto new_button = new_pad.buttons[button_id];
    if (button.pressed != new_button.pressed ||
        !GamepadButtonValuesAreEqual(button.value, new_button.value)) {
      send_frame = true;
      delegate->OnButton(button_id, new_button.pressed, new_button.value);
    }
  }
  if (send_frame)
    delegate->OnFrame();

  pad_state = new_pad;
}

}  // namespace exo
