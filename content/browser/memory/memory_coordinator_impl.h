// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_MEMORY_COORDINATOR_IMPL_H_
#define CONTENT_BROWSER_MEMORY_MEMORY_COORDINATOR_IMPL_H_

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/memory/memory_coordinator_client.h"
#include "base/memory/memory_coordinator_proxy.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator.mojom.h"
#include "content/public/browser/memory_coordinator.h"
#include "content/public/browser/memory_coordinator_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {

// NOTE: Memory coordinator is under development and not fully working.

class MemoryConditionObserver;
class MemoryCoordinatorHandleImpl;
class MemoryCoordinatorImplTest;
class MemoryMonitor;
class RenderProcessHost;
struct MemoryCoordinatorSingletonTraits;

// MemoryCondition is an internal state of memory coordinator which is used for
// various things; calculating memory state for processes, requesting processes
// to purge memory, and scheduling tab discarding.
enum class MemoryCondition : int {
  NORMAL = 0,
  WARNING = 1,
  CRITICAL = 2,
};

// MemoryCoordinatorImpl is an implementation of MemoryCoordinator.
class CONTENT_EXPORT MemoryCoordinatorImpl : public base::MemoryCoordinator,
                                             public MemoryCoordinator,
                                             public NotificationObserver,
                                             public base::NonThreadSafe {
 public:
  using MemoryState = base::MemoryState;

  static MemoryCoordinatorImpl* GetInstance();

  MemoryCoordinatorImpl(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                        std::unique_ptr<MemoryMonitor> monitor);
  ~MemoryCoordinatorImpl() override;

  MemoryMonitor* memory_monitor() { return memory_monitor_.get(); }

  // Starts monitoring memory usage. After calling this method, memory
  // coordinator will start dispatching state changes.
  void Start();

  // Called when the browser is foregrounded.
  void OnForegrounded();

  // Called when the browser is backgrounded.
  void OnBackgrounded();

  // Creates a handle to the provided child process.
  void CreateHandle(int render_process_id,
                    mojom::MemoryCoordinatorHandleRequest request);

  // Dispatches a memory state change to the provided process. Returns true if
  // the process is tracked by this coordinator and successfully dispatches,
  // returns false otherwise.
  bool SetChildMemoryState(int render_process_id, MemoryState memory_state);

  // Returns the memory state of the specified render process. Returns UNKNOWN
  // if the process is not tracked by this coordinator.
  MemoryState GetChildMemoryState(int render_process_id) const;

  // Records memory pressure notifications. Called by MemoryPressureMonitor.
  // TODO(bashi): Remove this when MemoryPressureMonitor is retired.
  void RecordMemoryPressure(
      base::MemoryPressureMonitor::MemoryPressureLevel level);

  // base::MemoryCoordinator implementations:
  MemoryState GetCurrentMemoryState() const override;

  // content::MemoryCoordinator implementation:
  MemoryState GetStateForProcess(base::ProcessHandle handle) override;

  // NotificationObserver implementation:
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  // Returns the current memory condition.
  MemoryCondition GetMemoryCondition() const { return memory_condition_; }

  // Overrides the current memory condition to |condition|. Memory condition
  // update tasks won't be scheduled until |duration| is passed. This means that
  // the memory condition remains the same until |duration| is passed or
  // another call of this method.
  void ForceSetMemoryCondition(MemoryCondition condition,
                               base::TimeDelta duration);

  // Changes current memory condition if needed. This may trigger some actions
  // like purging memory and memory state changes.
  void UpdateConditionIfNeeded(MemoryCondition condition);

  // Asks the delegate to discard a tab.
  void DiscardTab();

 protected:
  // Returns the RenderProcessHost which is correspond to the given id.
  // Returns nullptr if there is no corresponding RenderProcessHost.
  // This is a virtual method so that we can write tests without having
  // actual RenderProcessHost.
  virtual RenderProcessHost* GetRenderProcessHost(int render_process_id);

  // Sets a delegate for testing.
  void SetDelegateForTesting(
      std::unique_ptr<MemoryCoordinatorDelegate> delegate);

  MemoryCoordinatorDelegate* delegate() { return delegate_.get(); }

  // Adds the given ChildMemoryCoordinator as a child of this coordinator.
  void AddChildForTesting(int dummy_render_process_id,
                          mojom::ChildMemoryCoordinatorPtr child);

  // Sets a TickClock for testing.
  void SetTickClockForTesting(std::unique_ptr<base::TickClock> tick_clock);

  // Callback invoked by mojo when the child connection goes down. Exposed
  // for testing.
  void OnConnectionError(int render_process_id);

  // Returns true when a given renderer can be suspended.
  bool CanSuspendRenderer(int render_process_id);

  // Stores information about any known child processes.
  struct ChildInfo {
    // This object must be compatible with STL containers.
    ChildInfo();
    ChildInfo(const ChildInfo& rhs);
    ~ChildInfo();

    MemoryState memory_state;
    bool is_visible = false;
    std::unique_ptr<MemoryCoordinatorHandleImpl> handle;
  };

  // A map from process ID (RenderProcessHost::GetID()) to child process info.
  using ChildInfoMap = std::map<int, ChildInfo>;

  ChildInfoMap& children() { return children_; }

  base::SingleThreadTaskRunner* task_runner() { return task_runner_.get(); }

 private:
#if !defined(OS_MACOSX)
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplBrowserTest, HandleAdded);
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplBrowserTest,
                           CanSuspendRenderer);
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorWithServiceWorkerTest,
                           CannotSuspendRendererWithServiceWorker);
#endif
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, OnChildVisibilityChanged);
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, CalculateNextCondition);
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, UpdateCondition);
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, SetMemoryStateForTesting);
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, ForceSetMemoryCondition);
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, DiscardTabUnderCritical);

  friend struct MemoryCoordinatorSingletonTraits;
  friend class MemoryCoordinatorHandleImpl;

  // Called when ChildMemoryCoordinator calls AddChild().
  void OnChildAdded(int render_process_id);

  // Called when visibility of a child process is changed.
  void OnChildVisibilityChanged(int render_process_id, bool is_visible);

  // Called by SetChildMemoryState() to determine a child memory state based on
  // the current status of the child process.
  MemoryState OverrideState(MemoryState memroy_state, const ChildInfo& child);

  // Helper function of CreateHandle and AddChildForTesting.
  void CreateChildInfoMapEntry(
      int render_process_id,
      std::unique_ptr<MemoryCoordinatorHandleImpl> handle);

  // Updates the browser's memory state and notifies it to in-process clients.
  void UpdateBrowserStateAndNotifyStateToClients(MemoryState state);

  // Notifies a state change to in-process clients.
  void NotifyStateToClients(MemoryState state);

  // Notifies a state change to child processes.
  void NotifyStateToChildren(MemoryState state);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<MemoryCoordinatorDelegate> delegate_;
  std::unique_ptr<MemoryMonitor> memory_monitor_;
  std::unique_ptr<MemoryConditionObserver> condition_observer_;
  std::unique_ptr<base::TickClock> tick_clock_;
  NotificationRegistrar notification_registrar_;

  // The current memory condition.
  MemoryCondition memory_condition_ = MemoryCondition::NORMAL;

  // The memory state of the browser process.
  MemoryState browser_memory_state_ = MemoryState::NORMAL;

  // The time tick of last state change of the browser process.
  base::TimeTicks last_state_change_;

  // Memory state for a process will remain unchanged until this period of time
  // passes.
  base::TimeDelta minimum_state_transition_period_;

  // Used to delay setting browser's memory state. Cancelable to avoid executing
  // multiple tasks in the same time frame.
  base::CancelableClosure delayed_browser_memory_state_setter_;

  // Tracks child processes. An entry is added when a renderer connects to
  // MemoryCoordinator and removed automatically when an underlying binding is
  // disconnected.
  ChildInfoMap children_;

  DISALLOW_COPY_AND_ASSIGN(MemoryCoordinatorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_MEMORY_COORDINATOR_IMPL_H_
