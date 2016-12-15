// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_MEMORY_COORDINATOR_IMPL_H_
#define CONTENT_BROWSER_MEMORY_MEMORY_COORDINATOR_IMPL_H_

#include "base/callback.h"
#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "content/browser/memory/memory_coordinator.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {

class MemoryMonitor;
class MemoryCoordinatorImplTest;
class MemoryStateUpdater;
struct MemoryCoordinatorSingletonTraits;

// MemoryCoordinatorImpl is an implementation of MemoryCoordinator.
// The current implementation uses MemoryStateUpdater to update the global
// memory state. See comments in MemoryStateUpdater for details.
class CONTENT_EXPORT MemoryCoordinatorImpl : public MemoryCoordinator,
                                             public NotificationObserver,
                                             public base::NonThreadSafe {
 public:
  using MemoryState = base::MemoryState;

  MemoryCoordinatorImpl(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                        std::unique_ptr<MemoryMonitor> monitor);
  ~MemoryCoordinatorImpl() override;

  MemoryMonitor* memory_monitor() { return memory_monitor_.get(); }
  MemoryStateUpdater* state_updater() { return state_updater_.get(); }

  // MemoryCoordinator implementations:
  void Start() override;
  void OnChildAdded(int render_process_id) override;

  MemoryState GetGlobalMemoryState() const override;
  MemoryState GetCurrentMemoryState() const override;
  void SetCurrentMemoryStateForTesting(MemoryState memory_state) override;

  // NotificationObserver implementation:
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  // Overrides the global state to |new_state|. State update tasks won't be
  // scheduled until |duration| is passed. This means that the global state
  // remains the same until |duration| is passed or another call of this method.
  void ForceSetGlobalState(base::MemoryState new_state,
                           base::TimeDelta duration);

  // Changes the global state and notifies state changes to clients (lives in
  // the browser) and child processes (renderers) if needed. Returns true when
  // the state is actually changed.
  bool ChangeStateIfNeeded(MemoryState prev_state, MemoryState next_state);

 private:
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, CalculateNextState);
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, UpdateState);
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, SetMemoryStateForTesting);
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, ForceSetGlobalState);

  friend struct MemoryCoordinatorSingletonTraits;

  // Notifies a state change to in-process clients.
  void NotifyStateToClients();

  // Notifies a state change to child processes.
  void NotifyStateToChildren();

  // Records metrics. This is called when the global state is changed.
  void RecordStateChange(MemoryState prev_state,
                         MemoryState next_state,
                         base::TimeDelta duration);

  std::unique_ptr<MemoryMonitor> memory_monitor_;
  NotificationRegistrar notification_registrar_;
  std::unique_ptr<MemoryStateUpdater> state_updater_;
  MemoryState current_state_ = MemoryState::NORMAL;
  base::TimeTicks last_state_change_;

  DISALLOW_COPY_AND_ASSIGN(MemoryCoordinatorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_MEMORY_COORDINATOR_IMPL_H_
